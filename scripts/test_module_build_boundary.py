#!/usr/bin/env python3
"""Synthetic mutation checks for the post-build module boundary."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import test_module_build_boundaries as boundary


class ModuleBoundaryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="avemotion-module-boundary-")
        self.addCleanup(self.temp.cleanup)
        self.build = Path(self.temp.name)
        entries = {
            "CMAKE_BUILD_TYPE": "Release", "AVEMOTION_RLOTTIE_VARIANT": "none",
            "BUILD_TESTING": "OFF", "AVEMOTION_BUILD_DIRECT2D_BACKEND": "OFF",
            "AVEMOTION_ENABLE_INSTALL": "ON",
        }
        entries.update({name: "OFF" for name in boundary.TOOL_OPTIONS})
        self.cache = self.build / "CMakeCache.txt"
        self.cache.write_text("".join(f"{key}:STRING={value}\n" for key, value in entries.items()),
                              encoding="utf-8")
        for product in boundary.PRODUCT:
            (self.build / f"{product}.lib").write_bytes(b"archive")

    def test_valid_product_outputs(self) -> None:
        boundary.check(self.build, "none", False)

    def test_forbidden_reference_output(self) -> None:
        (self.build / "avemotion_reference.lib").write_bytes(b"lab")
        with self.assertRaisesRegex(ValueError, "unexpected.*archives"):
            boundary.check(self.build, "none", False)

    def test_unexpected_nonproduct_msvc_archive(self) -> None:
        (self.build / "extra.lib").write_bytes(b"forbidden")
        with self.assertRaisesRegex(ValueError, "unexpected.*archives"):
            boundary.check(self.build, "none", False)

    def test_unexpected_nonproduct_unix_archive(self) -> None:
        (self.build / "libsamsung_payload.a").write_bytes(b"forbidden")
        with self.assertRaisesRegex(ValueError, "unexpected.*archives"):
            boundary.check(self.build, "none", False)

    def test_missing_product_archive(self) -> None:
        (self.build / "avemotion_runtime.lib").unlink()
        with self.assertRaisesRegex(ValueError, "missing product archives"):
            boundary.check(self.build, "none", False)

    def test_stale_on_option(self) -> None:
        text = self.cache.read_text(encoding="utf-8")
        self.cache.write_text(text.replace("AVEMOTION_BUILD_PROBE:STRING=OFF",
                                           "AVEMOTION_BUILD_PROBE:STRING=ON"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "cache AVEMOTION_BUILD_PROBE"):
            boundary.check(self.build, "none", False)

    def test_built_lab_object_without_archive(self) -> None:
        object_path = self.build / "CMakeFiles/avemotion_reference.dir/src/reference/ReferenceRuntime.cpp.obj"
        object_path.parent.mkdir(parents=True)
        object_path.write_bytes(b"object")
        with self.assertRaisesRegex(ValueError, "laboratory/test objects were built"):
            boundary.check(self.build, "none", False)


if __name__ == "__main__":
    unittest.main(verbosity=2)
