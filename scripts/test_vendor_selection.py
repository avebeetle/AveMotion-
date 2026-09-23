#!/usr/bin/env python3
"""Selection and corpus regressions against a disposable vendor manifest."""
from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).with_name("verify_vendor.py")
spec = importlib.util.spec_from_file_location("verify_vendor", SCRIPT)
assert spec and spec.loader
vendor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vendor)


class VendorSelectionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        variants = {}
        for name in ("telegram", "samsung"):
            source = self.root / "third_party" / "rlottie" / name / "source"
            source.mkdir(parents=True)
            (source / "CMakeLists.txt").write_text("project(synthetic)\n", encoding="utf-8")
            (source / "COPYING").write_text(f"{name} test license\n", encoding="utf-8")
            (source.parent / "SOURCE_COMMIT").write_text(f"{name}-commit\n", encoding="utf-8")
            digest, files, byte_count = vendor.tree_fingerprint(source)
            variants[name] = {
                "source_directory": f"third_party/rlottie/{name}/source",
                "commit": f"{name}-commit",
                "source_tree_sha256": digest,
                "source_file_count": files,
                "source_byte_count": byte_count,
                "excluded_archive_artifacts": ["archive.zip"],
            }
        manifest = self.root / "third_party" / "rlottie" / "UPSTREAM.json"
        manifest.write_text(json.dumps({"variants": variants}), encoding="utf-8")
        corpus = self.root / "tests" / "corpus"
        corpus.mkdir(parents=True)
        self.fixture = corpus / "synthetic-only.json"
        self.fixture.write_bytes(b'{"synthetic": true}\n')
        digest = hashlib.sha256(self.fixture.read_bytes()).hexdigest()
        (corpus / "SHA256SUMS.txt").write_text(
            f"{digest}  tests/corpus/synthetic-only.json\n", encoding="utf-8"
        )

    def run_main(self, *args: str) -> tuple[int, str, str]:
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(vendor, "ROOT", self.root), patch.object(
            vendor, "MANIFEST", self.root / "third_party" / "rlottie" / "UPSTREAM.json", create=True
        ), patch.object(sys, "argv", [str(SCRIPT), *args]), redirect_stdout(stdout), redirect_stderr(stderr):
            try:
                result = vendor.main()
            except SystemExit as error:
                result = int(error.code)
        return result, stdout.getvalue(), stderr.getvalue()

    def source(self, name: str) -> Path:
        return self.root / "third_party" / "rlottie" / name / "source"

    def test_telegram_ignores_missing_samsung_tree(self) -> None:
        self.assertEqual(self.run_main()[0], 0)  # The synthetic fixture is valid under old behavior.
        self.source("samsung").rename(self.source("samsung").with_name("source-removed"))
        code, output, errors = self.run_main("--variant", "telegram")
        self.assertEqual(code, 0, errors)
        self.assertIn("Telegram", output)

    def test_all_and_default_require_both_vendor_trees(self) -> None:
        for selected in ((), ("--variant", "all")):
            for missing in ("telegram", "samsung"):
                with self.subTest(selected=selected, missing=missing):
                    self.assertEqual(self.run_main(*selected)[0], 0)
                    self.source(missing).rename(self.source(missing).with_name("source-removed"))
                    code, _, errors = self.run_main(*selected)
                    self.assertEqual(code, 1)
                    self.assertIn(f"{missing}: missing source CMakeLists.txt", errors)
                    self.source(missing).with_name("source-removed").rename(self.source(missing))

    def test_each_selection_checks_its_selected_tree(self) -> None:
        for selected in ("telegram", "samsung"):
            with self.subTest(selected=selected):
                tree = self.source(selected)
                (tree / "COPYING").write_text("corrupt", encoding="utf-8")
                errors = vendor.verify(self.root, selected)
                self.assertTrue(any(f"{selected}: source tree hash mismatch" in error for error in errors))
                (tree / "COPYING").write_text(f"{selected} test license\n", encoding="utf-8")
                self.assertEqual(vendor.verify(self.root, selected), [])

    def test_unselected_tree_can_be_corrupt_or_missing(self) -> None:
        for selected, other in (("telegram", "samsung"), ("samsung", "telegram")):
            with self.subTest(selected=selected):
                tree = self.source(other)
                (tree / "COPYING").write_text("corrupt", encoding="utf-8")
                self.assertEqual(vendor.verify(self.root, selected), [])
                tree.rename(tree.with_name("source-removed"))
                self.assertEqual(vendor.verify(self.root, selected), [])
                tree.with_name("source-removed").rename(tree)
                (tree / "COPYING").write_text(f"{other} test license\n", encoding="utf-8")

    def test_none_checks_corpus_without_vendor_trees(self) -> None:
        for name in ("telegram", "samsung"):
            self.source(name).rename(self.source(name).with_name("source-removed"))
        code, output, errors = self.run_main("--variant", "none")
        self.assertEqual(code, 0, errors)
        self.assertIn("corpus", output.lower())

    def test_corpus_corruption_fails_every_selection(self) -> None:
        self.fixture.write_bytes(b"corrupt\n")
        for selected in ("all", "telegram", "samsung", "none"):
            with self.subTest(selected=selected):
                self.assertIn(
                    "corpus hash mismatch: tests/corpus/synthetic-only.json",
                    vendor.verify(self.root, selected),
                )

    def test_missing_corpus_file_fails_even_without_vendor_trees(self) -> None:
        self.fixture.unlink()
        self.assertIn(
            "corpus hash mismatch: tests/corpus/synthetic-only.json",
            vendor.verify(self.root, "none"),
        )

    def test_selected_metadata_and_exclusion_checks_remain(self) -> None:
        tree = self.source("telegram")
        (tree.parent / "SOURCE_COMMIT").write_text("wrong", encoding="utf-8")
        (tree / "archive.zip").write_bytes(b"archive")
        (tree / "extra.txt").write_bytes(b"extra")
        (tree / "CMakeLists.txt").unlink()
        errors = vendor.verify(self.root, "telegram")
        self.assertTrue(any("SOURCE_COMMIT mismatch" in error for error in errors))
        self.assertTrue(any("excluded archive artifact" in error for error in errors))
        self.assertTrue(any("missing source CMakeLists.txt" in error for error in errors))
        self.assertTrue(any("source file count mismatch" in error for error in errors))

    def test_invalid_cli_selection_is_rejected_by_parser(self) -> None:
        code, output, errors = self.run_main("--variant", "typo")
        self.assertEqual(code, 2)
        self.assertEqual(output, "")
        self.assertIn("invalid choice", errors)
        with self.assertRaises(ValueError):
            vendor.verify(self.root, "typo")


if __name__ == "__main__":
    unittest.main()
