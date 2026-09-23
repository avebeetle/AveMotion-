#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path

def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"AveMotion corpus lab wiring test failed: {message}")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    args = parser.parse_args()
    cmake = (args.root / "CMakeLists.txt").read_text(encoding="utf-8")
    options = (args.root / "cmake/AveMotionOptions.cmake").read_text(encoding="utf-8")
    presets = (args.root / "CMakePresets.json").read_text(encoding="utf-8")
    runner = (args.root / "scripts/run_part24_corpus_lab.py").read_text(encoding="utf-8")
    require("AVEMOTION_BUILD_CORPUS_LAB" in options, "option missing")
    require("avemotion_corpus_analysis" in cmake, "analysis target missing")
    require("avemotion_corpus_lab" in cmake, "executable missing")
    require("avemotion.corpus.lab_smoke" in cmake, "smoke test missing")
    require((args.root / "scripts/run_part24_corpus_lab.py").is_file(),
            "safe runner missing")
    require('"linux-gcc-corpus-lab"' in presets,
            "dedicated Linux corpus preset missing")
    require('"windows-msvc-corpus-lab"' in presets,
            "dedicated Windows corpus preset missing")
    require('"windows-msvc-corpus-lab" if os.name == "nt"' in runner,
            "Windows runner default does not use the focused preset")
    require('else "linux-gcc-corpus-lab"' in runner,
            "Linux runner default does not use the focused preset")
    require((args.root / "docs/CORPUS_LAB.md").is_file(), "documentation missing")
    print("AveMotion corpus lab wiring verification passed")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
