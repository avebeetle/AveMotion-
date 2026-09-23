#!/usr/bin/env python3
"""Verify the Part 22 hardened TGS transport is wired as a standalone SDK layer."""

from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def main() -> int:
    cmake = read("CMakeLists.txt")
    tgs_header = read("include/avemotion/formats/Tgs.hpp")
    runtime_header = read("include/avemotion/runtime/Runtime.hpp")
    preview = read("apps/win32_preview/PreviewApp.cpp")

    match = re.search(r"project\(AveMotion\s+VERSION\s+(\d+)\.(\d+)\.(\d+)", cmake)
    require(match is not None and tuple(map(int, match.groups())) >= (0, 23, 0),
            "project version is older than the Part 23 transport baseline")
    require("add_library(AveMotion::Formats ALIAS avemotion_formats)" in cmake,
            "AveMotion::Formats target is missing")
    require("src/formats/Tgs.cpp" in cmake and "src/formats/MinizInflate.cpp" in cmake,
            "TGS/miniz implementation sources are not wired")
    require("AveMotion::Formats AveMotion::Model" in cmake,
            "Runtime does not publicly depend on Formats")
    require("install(TARGETS avemotion_formats" in cmake,
            "Formats target is not exported by the install package")
    require("avemotion.formats.tgs" in cmake and "avemotion.runtime.tgs" in cmake,
            "TGS tests are not registered")
    require("tests/fixtures/tgs/repeater_content_group.tgs" in cmake,
            "Win32 preview self-test is not using the TGS fixture")

    for api in ("decodeTgs(", "decodeTgsFile(", "detectAssetFormat("):
        require(api in tgs_header, f"public TGS API is missing {api}")
    require("miniz" not in tgs_header.lower() and "zlib" not in tgs_header.lower(),
            "third-party decompressor types leaked into the public TGS header")

    for api in ("loadTgs(", "loadTgsFile(", "loadAssetData("):
        require(api in runtime_header, f"Runtime is missing {api}")
    require("loadAssetData(bytes" in preview,
            "Win32 preview does not auto-detect JSON/TGS assets")
    require("asset.json|asset.tgs" in preview,
            "Win32 preview usage does not advertise TGS")
    require('L"repeater_content_group.tgs"' in preview,
            "Win32 preview default asset is not the TGS fixture")
    require((ROOT / "scripts/run_part22_windows.ps1").is_file()
            and (ROOT / "scripts/run_part22_windows.cmd").is_file(),
            "Part 22 Windows runner is missing")

    require((ROOT / "third_party/miniz/miniz.h").is_file(),
            "private miniz source is missing")
    require((ROOT / "LICENSES/UNLICENSE-miniz.txt").is_file(),
            "miniz license notice is missing")
    miniz_wrapper = read("src/formats/MinizInflate.cpp")
    require("avemotion_miniz_tinfl_decompress" in miniz_wrapper
            and "avemotion_miniz_mz_crc32" in miniz_wrapper,
            "private miniz symbols are not prefixed")
    require((ROOT / "tests/fixtures/tgs/repeater_content_group.tgs").is_file(),
            "TGS fixture is missing")

    print("AveMotion Part 22 TGS wiring verification passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
