#!/usr/bin/env python3
from __future__ import annotations

import csv
import importlib.util
import sys
import tempfile
from pathlib import Path


def load_module():
    script = Path(__file__).with_name("compare_source_geometry_golden.py")
    spec = importlib.util.spec_from_file_location("source_geometry_compare", script)
    if spec is None or spec.loader is None:
        raise RuntimeError("unable to load source-geometry comparator")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def write(path: Path, rows: list[dict[str, str]]) -> None:
    fields = [
        "variant",
        "asset",
        "asset_fnv64",
        "sample",
        "frame",
        "projected_fingerprint",
        "projected_portable_fingerprint",
        "visited",
        "points",
    ]
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    module = load_module()
    base = {
        "variant": "telegram",
        "asset": "polystar_polygon_geometry.json",
        "asset_fnv64": "1195b9b4b51fe504",
        "sample": "p000",
        "frame": "0",
        "projected_fingerprint": "7eee63a3feae85a5",
        "projected_portable_fingerprint": "1111111111111111",
        "visited": "8",
        "points": "177",
    }
    raw_varied = dict(base)
    raw_varied["projected_fingerprint"] = "c2aef5615ae7f285"

    with tempfile.TemporaryDirectory(prefix="avemotion-golden-policy-") as temp:
        root = Path(temp)
        expected = root / "expected.tsv"
        actual = root / "actual.tsv"
        write(expected, [base])
        write(actual, [raw_varied])

        matches, _, accepted = module.compare_files(
            expected, actual, allow_known_variance=False
        )
        if matches or accepted:
            raise RuntimeError("strict comparison accepted the synthetic variance")

        matches, errors, accepted = module.compare_files(
            expected, actual, allow_known_variance=True
        )
        if not matches or errors or accepted != 1:
            raise RuntimeError(
                "raw variance with an exact portable fingerprint was not accepted"
            )

        bad_portable = dict(raw_varied)
        bad_portable["projected_portable_fingerprint"] = "2222222222222222"
        write(actual, [bad_portable])
        matches, _, _ = module.compare_files(
            expected, actual, allow_known_variance=True
        )
        if matches:
            raise RuntimeError("portable geometry mismatch was hidden")

        bad_count = dict(raw_varied)
        bad_count["points"] = "176"
        write(actual, [bad_count])
        matches, _, _ = module.compare_files(
            expected, actual, allow_known_variance=True
        )
        if matches:
            raise RuntimeError("structural source-geometry change was hidden")

        non_telegram = dict(raw_varied)
        non_telegram["variant"] = "samsung"
        write(actual, [non_telegram])
        matches, _, _ = module.compare_files(
            expected, actual, allow_known_variance=True
        )
        if matches:
            raise RuntimeError("variance policy leaked to a non-Telegram row")

        write(expected, [base])
        # The comparator must also reject an older manifest schema that lacks
        # the independent portable fingerprint.
        legacy = root / "legacy.tsv"
        with legacy.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(
                stream,
                fieldnames=[
                    "variant",
                    "asset",
                    "asset_fnv64",
                    "sample",
                    "frame",
                    "projected_fingerprint",
                    "visited",
                    "points",
                ],
                delimiter="\t",
            )
            writer.writeheader()
            writer.writerow({
                key: value
                for key, value in base.items()
                if key != "projected_portable_fingerprint"
            })
        matches, _, _ = module.compare_files(
            legacy, legacy, allow_known_variance=True
        )
        if matches:
            raise RuntimeError("legacy manifest without portable identity passed")

    print("AveMotion cross-platform source-geometry golden policy passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
