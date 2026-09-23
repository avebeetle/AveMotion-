#!/usr/bin/env python3
"""Exercise the real CLI and runner against an existing report directory."""
from __future__ import annotations

import argparse
import csv
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def check_preserved(command: list[str], source: Path, directory: Path,
                    expected_warmup: int) -> None:
    with tempfile.TemporaryDirectory(prefix="avemotion-output-safety-") as temporary:
        output = Path(temporary) / "reports"
        sentinel = output / "nested" / "sentinel.txt"
        sentinel.parent.mkdir(parents=True)
        sentinel.write_text("keep this unrelated file", encoding="utf-8")
        copied_asset = output / source.name
        shutil.copy2(source, copied_asset)
        completed = subprocess.run(
            [part.replace("{output}", str(output))
             .replace("{asset}", str(copied_asset)) for part in command],
            cwd=directory, capture_output=True, text=True, check=False)
        if completed.returncode != 0:
            raise AssertionError(f"command failed ({completed.returncode}):\n"
                                 f"{completed.stdout}\n{completed.stderr}")
        assert sentinel.read_text(encoding="utf-8") == "keep this unrelated file"
        assert copied_asset.read_bytes() == source.read_bytes(), "input asset was removed"
        summary = (output / "corpus_summary.txt").read_text(encoding="utf-8")
        assert f"warmupSamplesPerAsset={expected_warmup}" in summary


def check_memory_mode(root: Path, source: Path, preset: str) -> None:
    with tempfile.TemporaryDirectory(prefix="avemotion-memory-safety-") as temporary:
        output = Path(temporary) / "reports"
        output.mkdir()
        copied_asset = output / source.name
        shutil.copy2(source, copied_asset)
        sentinel = output / "keep.txt"
        sentinel.write_text("keep", encoding="utf-8")
        completed = subprocess.run(
            [sys.executable, str(root / "scripts/run_part24_corpus_lab.py"),
             "--input-dir", str(output), "--output", str(output),
             "--skip-build", "--preset", preset, "--memory-instances", "1"],
            cwd=root, capture_output=True, text=True, check=False)
        if os.name != "nt":
            assert completed.returncode != 0
            assert "requires Windows" in completed.stderr
        else:
            assert completed.returncode == 0, completed.stdout + completed.stderr
            with (output / "memory_observation.tsv").open(
                    encoding="utf-8", newline="") as stream:
                rows = list(csv.DictReader(stream, delimiter="\t"))
            assert len(rows) == 1
            assert set(rows[0]) == {"asset", "instances", "working_set_bytes",
                                    "peak_working_set_bytes"}
            assert rows[0]["asset"].startswith("asset-")
            assert rows[0]["instances"] == "1"
            current = int(rows[0]["working_set_bytes"])
            peak = int(rows[0]["peak_working_set_bytes"])
            assert 0 < current <= peak
        assert copied_asset.read_bytes() == source.read_bytes()
        assert sentinel.read_text(encoding="utf-8") == "keep"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    source = root / "tests/compatibility/tgs/StickAndBall.tgs"
    common = ["--samples", "1", "--load-repeats", "1", "--cpu-repeats", "0",
              "--render-size", "64", "--strict"]
    check_preserved([str(args.executable.resolve()), "--input", "{asset}",
                     "--output", "{output}", *common], source, root, 20)
    check_preserved([sys.executable, str(root / "scripts/run_part24_corpus_lab.py"),
                     "--input-dir", "{output}", "--output", "{output}",
                     "--skip-build", "--preset", args.executable.resolve().parent.name,
                     *common, "--warmup-samples", "0"], source, root, 0)
    check_memory_mode(root, source, args.executable.resolve().parent.name)
    print("AveMotion corpus lab output preservation passed")


if __name__ == "__main__":
    main()
