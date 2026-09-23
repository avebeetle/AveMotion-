#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import tempfile

SCRIPT = Path(__file__).resolve().with_name("verify_vendor.py")
spec = importlib.util.spec_from_file_location("verify_vendor", SCRIPT)
if spec is None or spec.loader is None:
    raise RuntimeError("Could not load verify_vendor.py")
verify_vendor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify_vendor)


def fingerprint_in_order(root: Path, paths: list[Path]) -> str:
    digest = hashlib.sha256()
    for file in paths:
        relative = file.relative_to(root).as_posix().encode("utf-8")
        data = file.read_bytes()
        digest.update(len(relative).to_bytes(4, "little"))
        digest.update(relative)
        digest.update(len(data).to_bytes(8, "little"))
        digest.update(data)
    return digest.hexdigest()


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="avemotion-vendor-order-") as directory:
        root = Path(directory)
        fixtures = {
            "CMakeLists.txt": b"root\n",
            "cmake/config.cmake": b"config\n",
            "Inc/Header.h": b"upper\n",
            "inc/source.h": b"lower\n",
        }
        for relative, data in fixtures.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

        files = [path for path in root.rglob("*") if path.is_file()]
        canonical = sorted(
            files,
            key=lambda path: path.relative_to(root).as_posix().encode("utf-8"),
        )
        windows_like = sorted(
            files,
            key=lambda path: path.relative_to(root).as_posix().casefold(),
        )
        expected = fingerprint_in_order(root, canonical)
        legacy_windows_hash = fingerprint_in_order(root, windows_like)
        actual, file_count, byte_count = verify_vendor.tree_fingerprint(root)

        if expected == legacy_windows_hash:
            raise AssertionError("Fixture did not distinguish canonical and Windows-like order")
        if actual != expected:
            raise AssertionError(
                f"Canonical fingerprint mismatch: expected {expected}, got {actual}")
        if file_count != len(fixtures):
            raise AssertionError(f"Expected {len(fixtures)} files, got {file_count}")
        if byte_count != sum(len(data) for data in fixtures.values()):
            raise AssertionError("Byte count mismatch")

    print("Vendor fingerprint ordering regression test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
