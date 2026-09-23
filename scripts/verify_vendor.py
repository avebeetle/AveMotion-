#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "third_party" / "rlottie" / "UPSTREAM.json"


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical_tree_files(path: Path) -> list[tuple[bytes, Path]]:
    """Return files in one OS-independent, case-sensitive path order.

    Sorting ``Path`` objects directly is not portable: ``WindowsPath`` uses
    case-insensitive path semantics while ``PosixPath`` is case-sensitive.
    A tree containing names such as ``CMakeLists.txt`` and ``cmake/...`` can
    therefore produce a different aggregate hash on Windows even when every
    file byte is identical.  The fingerprint format is defined in terms of
    UTF-8 encoded POSIX relative paths, so order by those exact bytes.
    """
    entries: list[tuple[bytes, Path]] = []
    for candidate in path.rglob("*"):
        if not candidate.is_file():
            continue
        relative = candidate.relative_to(path).as_posix().encode("utf-8")
        entries.append((relative, candidate))
    entries.sort(key=lambda entry: entry[0])
    return entries


def fingerprint_entries(entries: Iterable[tuple[bytes, Path]]) -> tuple[str, int, int]:
    digest = hashlib.sha256()
    file_count = 0
    byte_count = 0
    for relative, file in entries:
        data = file.read_bytes()
        digest.update(len(relative).to_bytes(4, "little"))
        digest.update(relative)
        digest.update(len(data).to_bytes(8, "little"))
        digest.update(data)
        file_count += 1
        byte_count += len(data)
    return digest.hexdigest(), file_count, byte_count


def tree_fingerprint(path: Path) -> tuple[str, int, int]:
    """Hash relative POSIX paths and bytes, independent of host OS metadata."""
    return fingerprint_entries(canonical_tree_files(path))


def main() -> int:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    errors: list[str] = []
    for name, entry in data["variants"].items():
        source = ROOT / entry["source_directory"]
        commit_file = source.parent / "SOURCE_COMMIT"
        if not (source / "CMakeLists.txt").is_file():
            errors.append(f"{name}: missing source CMakeLists.txt")
        if not (source / "COPYING").is_file():
            errors.append(f"{name}: missing COPYING")
        if not commit_file.is_file() or commit_file.read_text().strip() != entry["commit"]:
            errors.append(f"{name}: SOURCE_COMMIT mismatch")
        for excluded in entry.get("excluded_archive_artifacts", []):
            if (source / excluded).exists():
                errors.append(f"{name}: excluded archive artifact is present: {excluded}")
        if source.is_dir():
            actual_hash, actual_files, actual_bytes = tree_fingerprint(source)
            if actual_hash != entry.get("source_tree_sha256"):
                errors.append(
                    f"{name}: source tree hash mismatch: expected "
                    f"{entry.get('source_tree_sha256')}, got {actual_hash}")
            if actual_files != entry.get("source_file_count"):
                errors.append(
                    f"{name}: source file count mismatch: expected "
                    f"{entry.get('source_file_count')}, got {actual_files}")
            if actual_bytes != entry.get("source_byte_count"):
                errors.append(
                    f"{name}: source byte count mismatch: expected "
                    f"{entry.get('source_byte_count')}, got {actual_bytes}")

    sums = ROOT / "tests" / "corpus" / "SHA256SUMS.txt"
    for line in sums.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        expected, relative = line.split(maxsplit=1)
        relative = relative.lstrip("*")
        path = ROOT / relative
        if not path.is_file() or file_sha256(path) != expected:
            errors.append(f"corpus hash mismatch: {relative}")

    if errors:
        print("Vendor verification failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print(
        "Vendor verification passed for Telegram primary and Samsung comparison variants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
