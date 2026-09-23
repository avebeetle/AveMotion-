#!/usr/bin/env python3
"""Verify protected repository paths survive a real Git add/checkout roundtrip."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


CASES = {
    "third_party/probe/source.cpp": b"first\nsecond\n",
    "third_party/probe/project.sln": b"first\r\nsecond\r\n",
    "patches/probe/change.patch": b"first\nsecond\n",
    "tests/corpus/probe.json": b'{"value":1}\n',
    "tests/compatibility/tgs/SHA256SUMS.txt": b"hash  probe.tgs\n",
    "tests/compatibility/tgs/probe.tgs": b"\x00\x0a\x0d\xff",
    "tests/fixtures/probe.json": b'{"value":1}\n',
    "tests/fixtures/reference_sessions/probe.json": b'{"value":2}\n',
    "tests/fixtures/tgs/SHA256SUMS.txt": b"hash  probe.tgs\n",
    "tests/fixtures/tgs/probe.tgs": b"\x00\x0a\x0d\xff",
}


def git(executable: str, repository: Path, autocrlf: str, *arguments: str) -> None:
    command = [executable, "-C", str(repository), "-c", f"core.autocrlf={autocrlf}", *arguments]
    subprocess.run(command, check=True, capture_output=True, text=True)


def roundtrip(executable: str, attributes: Path, autocrlf: str) -> list[str]:
    with tempfile.TemporaryDirectory(prefix="avemotion-git-protected-") as temporary:
        root = Path(temporary)
        repository = root / "repository"
        checkout = root / "checkout"
        repository.mkdir()
        checkout.mkdir()
        git(executable, repository, autocrlf, "init", "-q")
        if attributes.is_file():
            shutil.copyfile(attributes, repository / ".gitattributes")
        for relative, expected in CASES.items():
            destination = repository / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(expected)
        git(executable, repository, autocrlf, "add", "--", ".")
        git(executable, repository, autocrlf, "checkout-index", f"--prefix={checkout}{os.sep}", "--all")
        errors = []
        for relative, expected in CASES.items():
            actual = (checkout / relative).read_bytes()
            if actual != expected:
                errors.append(f"autocrlf={autocrlf} {relative}: expected {expected!r}, got {actual!r}")
        return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--git", required=True, help="Git executable to use")
    parser.add_argument("--attributes", required=True, type=Path, help="source .gitattributes path")
    arguments = parser.parse_args()
    errors = []
    for mode in ("true", "false"):
        errors.extend(roundtrip(arguments.git, arguments.attributes, mode))
    if errors:
        print("Protected Git byte roundtrip failed:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Protected Git byte roundtrip passed with core.autocrlf=true and false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
