#!/usr/bin/env python3
from __future__ import annotations
import argparse
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tempfile
import zipfile

MAX_ZIP_FILES = 1000
MAX_ZIP_EXPANDED = 64 * 1024 * 1024

def fail(message: str) -> "NoReturn":
    raise SystemExit(f"AveMotion Part 24 runner failed: {message}")

def run(command: list[str], cwd: Path) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    completed = subprocess.run(command, cwd=cwd, check=False)
    if completed.returncode != 0:
        fail(f"command returned {completed.returncode}: {command[0]}")

def safe_extract(archive: Path, destination: Path) -> Path:
    with zipfile.ZipFile(archive) as source:
        entries = source.infolist()
        if len(entries) > MAX_ZIP_FILES:
            fail(f"ZIP has more than {MAX_ZIP_FILES} entries")
        expanded = 0
        for entry in entries:
            name = PurePosixPath(entry.filename)
            if name.is_absolute() or ".." in name.parts:
                fail(f"unsafe ZIP path: {entry.filename}")
            mode = (entry.external_attr >> 16) & 0xFFFF
            if stat.S_ISLNK(mode):
                fail(f"ZIP symlink is not allowed: {entry.filename}")
            expanded += entry.file_size
            if expanded > MAX_ZIP_EXPANDED:
                fail("ZIP expanded size exceeds 64 MiB")
        source.extractall(destination)
    return destination

def executable_path(root: Path, preset: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    path = root / "out" / "build" / preset / f"avemotion_corpus_lab{suffix}"
    if not path.is_file():
        fail(f"corpus lab executable was not produced: {path}")
    return path

def main() -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--input-dir", type=Path)
    group.add_argument("--input-zip", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--preset")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--include-names", action="store_true")
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--samples", type=int, default=60)
    parser.add_argument("--load-repeats", type=int, default=3)
    parser.add_argument("--cpu-repeats", type=int, default=2)
    parser.add_argument("--render-size", type=int, default=128)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    preset = args.preset or (
        "windows-msvc-corpus-lab" if os.name == "nt"
        else "linux-gcc-corpus-lab")
    if not args.skip_build:
        run(["cmake", "--preset", preset], root)
        run(["cmake", "--build", "--preset", preset,
             "--target", "avemotion_corpus_lab"], root)
    executable = executable_path(root, preset)
    temporary: tempfile.TemporaryDirectory[str] | None = None
    if args.input_zip is not None:
        if not args.input_zip.is_file():
            fail(f"input ZIP does not exist: {args.input_zip}")
        temporary = tempfile.TemporaryDirectory(prefix="avemotion-corpus-")
        input_path = safe_extract(args.input_zip, Path(temporary.name))
    else:
        input_path = args.input_dir
        if input_path is None or not input_path.exists():
            fail(f"input directory does not exist: {input_path}")
    if args.output.exists():
        shutil.rmtree(args.output)
    command = [str(executable), "--input", str(input_path),
               "--output", str(args.output), "--samples", str(args.samples),
               "--load-repeats", str(args.load_repeats),
               "--cpu-repeats", str(args.cpu_repeats),
               "--render-size", str(args.render_size)]
    if args.include_names:
        command.append("--include-names")
    if args.strict:
        command.append("--strict")
    run(command, root)
    if temporary is not None:
        temporary.cleanup()
    print(f"AveMotion Part 24 reports: {args.output}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
