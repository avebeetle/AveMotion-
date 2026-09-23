#!/usr/bin/env python3
"""Check actual default-build outputs for a lean AveMotion module profile."""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

PRODUCT = {
    "avemotion_formats", "avemotion_core", "avemotion_model",
    "avemotion_validation", "avemotion_evaluation", "avemotion_runtime",
    "avemotion_player", "avemotion_rendering",
}
TOOL_OPTIONS = {
    "AVEMOTION_BUILD_PROBE", "AVEMOTION_BUILD_VALIDATOR_TOOL",
    "AVEMOTION_BUILD_VALIDATION_CHARACTERIZER", "AVEMOTION_BUILD_CORPUS_LAB",
    "AVEMOTION_BUILD_CHARACTERIZER", "AVEMOTION_BUILD_SCENE_CHARACTERIZER",
    "AVEMOTION_BUILD_PLAN_CHARACTERIZER", "AVEMOTION_BUILD_MODEL_CHARACTERIZER",
    "AVEMOTION_BUILD_PROPERTY_CHARACTERIZER",
    "AVEMOTION_BUILD_SOURCE_GEOMETRY_CHARACTERIZER",
    "AVEMOTION_BUILD_DIRECT2D_CONTRACT_TEST",
    "AVEMOTION_BUILD_DIRECT2D_CAPTURE_TEST", "AVEMOTION_BUILD_WIN32_PREVIEW",
}


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def cache_values(path: Path) -> dict[str, str]:
    require(path.is_file(), f"missing CMake cache: {path}")
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([^:=]+):[^=]*=(.*)", line)
        if match:
            values[match.group(1)] = match.group(2)
    return values


def archive_stem(path: Path) -> str | None:
    name = path.name.lower()
    if name.endswith(".lib"):
        return name[:-4]
    if name.startswith("lib") and name.endswith(".a"):
        return name[3:-2]
    return None


def check(build_dir: Path, variant: str, direct2d: bool) -> None:
    build_dir = build_dir.resolve(strict=True)
    cache = cache_values(build_dir / "CMakeCache.txt")
    expected = {
        "CMAKE_BUILD_TYPE": "Release",
        "AVEMOTION_RLOTTIE_VARIANT": variant,
        "BUILD_TESTING": "OFF",
        "AVEMOTION_BUILD_DIRECT2D_BACKEND": "ON" if direct2d else "OFF",
        "AVEMOTION_ENABLE_INSTALL": "OFF" if variant == "telegram" else "ON",
    }
    for name in TOOL_OPTIONS:
        expected[name] = "OFF"
    for name, value in expected.items():
        require(cache.get(name) == value, f"cache {name}: expected {value}, got {cache.get(name)!r}")

    required = set(PRODUCT)
    if direct2d:
        required.add("avemotion_backend_d2d")
    archives = {stem for path in build_dir.rglob("*") if path.is_file()
                for stem in (archive_stem(path),) if stem is not None}
    missing = required - archives
    require(not missing, f"missing product archives: {sorted(missing)}")
    has_rlottie = "rlottie" in archives
    require(has_rlottie == (variant == "telegram"),
            f"selected upstream archive mismatch: rlottie present={has_rlottie}")
    allowed = required | ({"rlottie"} if variant == "telegram" else set())
    extra = archives - allowed
    require(not extra, f"unexpected archives: {sorted(extra)}")
    forbidden_exe = [str(path.relative_to(build_dir)) for path in build_dir.rglob("*")
                     if path.is_file() and path.suffix.lower() in (".exe", "")
                     and path.name.lower().startswith("avemotion_")
                     and (path.suffix.lower() == ".exe" or path.stat().st_mode & 0o111)]
    require(not forbidden_exe, f"unexpected AveMotion executables: {forbidden_exe}")
    forbidden_objects = []
    for path in build_dir.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in (".obj", ".o"):
            continue
        parts = path.relative_to(build_dir).parts
        for part in parts:
            if part.startswith("avemotion_") and part.endswith(".dir"):
                target = part[:-4]
                if target not in required and target != "avemotion_miniz":
                    forbidden_objects.append(str(path.relative_to(build_dir)))
                break
    require(not forbidden_objects, f"laboratory/test objects were built: {forbidden_objects[:8]}")
    print(f"MODULE_BOUNDARY_OK variant={variant} direct2d={'yes' if direct2d else 'no'} "
          f"archives={','.join(sorted(required))}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--variant", required=True, choices=("telegram", "none"))
    parser.add_argument("--direct2d", required=True, choices=("yes", "no"))
    args = parser.parse_args()
    try:
        check(args.build_dir, args.variant, args.direct2d == "yes")
    except (OSError, ValueError) as error:
        print(f"MODULE_BOUNDARY_REJECTED: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
