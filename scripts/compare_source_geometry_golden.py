#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import sys
from dataclasses import dataclass
from pathlib import Path

RAW_FINGERPRINT_FIELD = "projected_fingerprint"
PORTABLE_FINGERPRINT_FIELD = "projected_portable_fingerprint"


@dataclass(frozen=True)
class Comparison:
    matches: bool
    accepted_known_variance: bool = False
    message: str = ""


def load(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        if reader.fieldnames is None:
            raise ValueError(f"{path} has no TSV header")
        return list(reader.fieldnames), list(reader)


def compare_row(
    expected: dict[str, str],
    actual: dict[str, str],
    *,
    allow_known_variance: bool,
) -> Comparison:
    differing = {
        field
        for field in expected
        if expected.get(field) != actual.get(field)
    }
    differing.update(field for field in actual if field not in expected)
    if not differing:
        return Comparison(True)

    # The raw fingerprint intentionally preserves exact IEEE-754 bits. Legacy
    # Telegram rlottie geometry can differ by a few ULPs between libm and the
    # MSVC CRT. On an MSVC build only, accept a raw-fingerprint difference when
    # every semantic/structural field is still exact and the independently
    # generated quantized geometry fingerprint matches byte-for-byte.
    if (
        allow_known_variance
        and expected.get("variant") == "telegram"
        and actual.get("variant") == "telegram"
        and differing == {RAW_FINGERPRINT_FIELD}
        and expected.get(PORTABLE_FINGERPRINT_FIELD)
        and expected.get(PORTABLE_FINGERPRINT_FIELD)
            == actual.get(PORTABLE_FINGERPRINT_FIELD)
    ):
        return Comparison(
            True,
            True,
            "accepted raw-float fingerprint variance with exact portable "
            "geometry fingerprint",
        )

    return Comparison(
        False,
        False,
        "differing fields: " + ", ".join(sorted(differing)),
    )


def compare_files(
    expected_path: Path,
    actual_path: Path,
    *,
    allow_known_variance: bool,
) -> tuple[bool, list[str], int]:
    expected_fields, expected_rows = load(expected_path)
    actual_fields, actual_rows = load(actual_path)
    errors: list[str] = []
    accepted = 0

    if expected_fields != actual_fields:
        errors.append(
            "header differs:\n"
            f"  expected: {expected_fields}\n"
            f"  actual:   {actual_fields}"
        )
        return False, errors, accepted
    if PORTABLE_FINGERPRINT_FIELD not in expected_fields:
        errors.append(
            f"required field is missing: {PORTABLE_FINGERPRINT_FIELD}"
        )
        return False, errors, accepted
    if len(expected_rows) != len(actual_rows):
        errors.append(
            f"row count differs: expected {len(expected_rows)}, "
            f"got {len(actual_rows)}"
        )
        return False, errors, accepted

    for index, (expected, actual) in enumerate(zip(expected_rows, actual_rows)):
        comparison = compare_row(
            expected,
            actual,
            allow_known_variance=allow_known_variance,
        )
        if comparison.matches:
            if comparison.accepted_known_variance:
                accepted += 1
            continue
        key = (
            expected.get("variant", "?"),
            expected.get("asset", "?"),
            expected.get("sample", "?"),
        )
        errors.append(
            f"row {index} {key}: {comparison.message}\n"
            f"  expected: {expected}\n"
            f"  actual:   {actual}"
        )
        break

    return not errors, errors, accepted


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--expected", type=Path, required=True)
    parser.add_argument("--actual", type=Path, required=True)
    parser.add_argument(
        "--allow-telegram-msvc-raw-float-variance",
        action="store_true",
        help=(
            "allow only raw projected-fingerprint differences when the "
            "portable quantized geometry fingerprint and every other field "
            "remain exact"
        ),
    )
    # Compatibility alias for Part 20.3 build trees and external scripts.
    parser.add_argument(
        "--allow-telegram-msvc-endpoint-float-variance",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    args = parser.parse_args()

    try:
        matches, errors, accepted = compare_files(
            args.expected,
            args.actual,
            allow_known_variance=(
                args.allow_telegram_msvc_raw_float_variance
                or args.allow_telegram_msvc_endpoint_float_variance
            ),
        )
    except (OSError, ValueError, csv.Error) as error:
        print(f"Source-geometry golden comparison failed: {error}", file=sys.stderr)
        return 1

    if not matches:
        print("Source-geometry golden mismatch", file=sys.stderr)
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(
        "AveMotion source-geometry golden comparison passed"
        + (
            f" ({accepted} portable-geometry-confirmed MSVC raw-float "
            "variance)"
            if accepted
            else ""
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
