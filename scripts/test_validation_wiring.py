#!/usr/bin/env python3
"""Verify Part 23 validation and deterministic TGS compatibility wiring."""

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
    options = read("cmake/AveMotionOptions.cmake")
    header = read("include/avemotion/validation/AssetValidator.hpp")
    implementation = read("src/validation/AssetValidator.cpp")
    consumer = read("tests/consumer/main.cpp")
    workflow = read(".github/workflows/ci.yml")

    match = re.search(r"project\(AveMotion\s+VERSION\s+(\d+)\.(\d+)\.(\d+)", cmake)
    require(match is not None and tuple(map(int, match.groups())) >= (0, 23, 0),
            "project version is older than the Part 23 validation baseline")
    for fragment in (
        "add_library(avemotion_validation STATIC",
        "add_library(AveMotion::Validation ALIAS avemotion_validation)",
        "EXPORT_NAME Validation",
        "install(TARGETS avemotion_formats avemotion_core avemotion_model avemotion_validation",
    ):
        require(fragment in cmake, f"missing validation target wiring: {fragment}")

    for test_name in (
        "avemotion.cmake.validation_wiring",
        "avemotion.validation.asset",
        "avemotion.validation.tgs_corpus_integrity",
        "avemotion.validation.compatibility_corpus",
        "avemotion.validation.characterize",
        "avemotion.validation.golden",
        "avemotion.validation.feature_golden",
        "avemotion.validation.summary_golden",
    ):
        require(test_name in cmake, f"missing validation test: {test_name}")

    require("AVEMOTION_BUILD_VALIDATOR_TOOL" in options,
            "validator CLI build option is missing")
    require("AVEMOTION_BUILD_VALIDATION_CHARACTERIZER" in options,
            "validation characterizer build option is missing")
    require("apps/validate/main.cpp" in cmake,
            "validator CLI source is not wired")
    require("apps/validation_characterize/main.cpp" in cmake,
            "validation characterizer source is not wired")

    for api in (
        "class AssetValidator final",
        "AssetValidationReport",
        "AssetValidationOptions",
        "ValidationProfile",
        "FeatureUsage",
        "ValidationIssue",
    ):
        require(api in header, f"public validation API is missing {api}")
    public_lower = header.lower()
    require("rlottie" not in public_lower and "windows.h" not in public_lower,
            "reference/platform types leaked into the validation public header")

    require("telegramSticker()" in implementation,
            "Telegram authoring profile implementation is missing")
    require("direct2DNative()" in implementation,
            "Direct2D-native profile implementation is missing")
    require("forbiddenByTelegramProfile" in implementation,
            "Telegram forbidden-feature policy is missing")

    require((ROOT / "tests/compatibility/manifest.tsv").is_file(),
            "compatibility manifest is missing")
    require((ROOT / "tests/compatibility/tgs/SHA256SUMS.txt").is_file(),
            "compatibility checksum manifest is missing")
    require((ROOT / "tests/golden/validation-telegram.tsv").is_file(),
            "validation golden manifest is missing")
    require((ROOT / "tests/golden/validation-features-telegram.tsv").is_file(),
            "validation feature-frequency golden is missing")
    require((ROOT / "tests/golden/validation-summary-telegram.txt").is_file(),
            "validation summary golden is missing")
    require((ROOT / "tests/fixtures/telegram_sticker_basic.json").is_file(),
            "strict Telegram-profile fixture is missing")
    require((ROOT / "scripts/generate_tgs_compatibility_corpus.py").is_file(),
            "deterministic corpus generator is missing")
    require((ROOT / "scripts/run_part23_windows.ps1").is_file()
            and (ROOT / "scripts/run_part23_windows.cmd").is_file(),
            "Part 23 Windows runner is missing")

    require("AveMotion::Validation" in read("tests/consumer/CMakeLists.txt"),
            "installed-package consumer does not link Validation")
    require("AssetValidator" in consumer,
            "installed-package consumer does not compile the validation API")
    require("run_part23_windows.ps1" in workflow,
            "Windows CI is not routed through the Part 23 validation gate")

    manifest_lines = (ROOT / "tests/compatibility/manifest.tsv").read_text(
        encoding="utf-8").splitlines()
    require(len(manifest_lines) >= 3,
            "compatibility corpus is unexpectedly empty")
    require(any("telegram_sticker_basic.tgs" in line for line in manifest_lines),
            "strict Telegram-profile TGS fixture is not in the compatibility corpus")

    print("AveMotion Part 23 validation wiring verification passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
