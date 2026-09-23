#!/usr/bin/env python3
"""Generate or verify the deterministic AveMotion TGS compatibility corpus."""

from __future__ import annotations

import argparse
import binascii
import struct
import hashlib
from pathlib import Path
import sys


def source_files(root: Path) -> list[Path]:
    files = list((root / "tests" / "corpus").glob("*.json"))
    files += list((root / "tests" / "fixtures").glob("*.json"))
    return sorted(files, key=lambda path: path.as_posix().encode("utf-8"))


def build_payload(data: bytes) -> bytes:
    """Build a byte-for-byte portable gzip stream with stored DEFLATE blocks.

    Python/zlib compressor output is not a stable file format contract across
    zlib versions and operating systems.  The compatibility corpus needs exact
    regeneration on Linux, Windows and macOS, so it uses valid uncompressed
    DEFLATE blocks inside a canonical RFC 1952 envelope.  Dynamic-Huffman TGS
    coverage remains in the dedicated Part 22 fixtures and decoder tests.
    """
    output = bytearray(b"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff")
    if not data:
        output.extend(b"\x01\x00\x00\xff\xff")
    else:
        offset = 0
        while offset < len(data):
            chunk = data[offset : offset + 0xFFFF]
            offset += len(chunk)
            output.append(0x01 if offset == len(data) else 0x00)
            length = len(chunk)
            output.extend(struct.pack("<HH", length, length ^ 0xFFFF))
            output.extend(chunk)
    output.extend(struct.pack("<II", binascii.crc32(data) & 0xFFFFFFFF, len(data) & 0xFFFFFFFF))
    return bytes(output)


def expected_outputs(root: Path) -> tuple[list[tuple[Path, bytes]], str, str]:
    outputs: list[tuple[Path, bytes]] = []
    rows = ["source_json\ttgs\tjson_bytes\ttgs_bytes\tsha256"]
    sums: list[str] = []
    output_dir = root / "tests" / "compatibility" / "tgs"
    seen_stems: set[str] = set()
    for source in source_files(root):
        if source.stem in seen_stems:
            raise ValueError(f"duplicate compatibility-corpus stem: {source.stem}")
        seen_stems.add(source.stem)
        data = source.read_bytes()
        payload = build_payload(data)
        target = output_dir / f"{source.stem}.tgs"
        digest = hashlib.sha256(payload).hexdigest()
        outputs.append((target, payload))
        rows.append(
            "\t".join(
                (
                    source.relative_to(root).as_posix(),
                    target.relative_to(root).as_posix(),
                    str(len(data)),
                    str(len(payload)),
                    digest,
                )
            )
        )
        sums.append(f"{digest}  {target.name}")
    return outputs, "\n".join(rows) + "\n", "\n".join(sums) + "\n"


def write(root: Path) -> None:
    outputs, manifest, sums = expected_outputs(root)
    for path, payload in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)
    compatibility = root / "tests" / "compatibility"
    compatibility.mkdir(parents=True, exist_ok=True)
    (compatibility / "manifest.tsv").write_text(manifest, encoding="utf-8")
    (compatibility / "tgs" / "SHA256SUMS.txt").write_text(
        sums, encoding="utf-8"
    )


def check(root: Path) -> int:
    outputs, manifest, sums = expected_outputs(root)
    failures: list[str] = []
    expected_paths = {path.resolve() for path, _ in outputs}
    output_dir = root / "tests" / "compatibility" / "tgs"
    actual_paths = {
        path.resolve()
        for path in output_dir.glob("*.tgs")
        if path.is_file()
    }
    for extra in sorted(actual_paths - expected_paths):
        failures.append(f"unexpected TGS file: {extra}")
    for path, payload in outputs:
        if not path.exists():
            failures.append(f"missing TGS file: {path}")
        elif path.read_bytes() != payload:
            failures.append(f"TGS payload mismatch: {path}")
    manifest_path = root / "tests" / "compatibility" / "manifest.tsv"
    sums_path = output_dir / "SHA256SUMS.txt"
    if not manifest_path.exists() or manifest_path.read_text(encoding="utf-8") != manifest:
        failures.append("compatibility manifest mismatch")
    if not sums_path.exists() or sums_path.read_text(encoding="utf-8") != sums:
        failures.append("compatibility SHA256SUMS mismatch")
    if failures:
        print("AveMotion TGS compatibility corpus verification failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print(f"AveMotion TGS compatibility corpus verified: {len(outputs)} assets")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    if args.check:
        return check(root)
    write(root)
    print(f"Generated {len(source_files(root))} deterministic TGS assets")
    return 0


if __name__ == "__main__":
    sys.exit(main())
