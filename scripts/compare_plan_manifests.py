#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path

FIELDS = (
    "plan",
    "topology",
    "geometry_identity",
    "paint_identity",
    "presentation",
    "frame",
    "visible_items",
    "geometry_updates",
    "paint_updates",
    "unsupported_items",
)


def load(path: Path) -> dict[tuple[str, str], dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        return {
            (row["asset"], row["sample"]): row
            for row in csv.DictReader(stream, delimiter="\t")
        }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    left = load(args.left)
    right = load(args.right)
    keys = sorted(set(left) | set(right))
    differences = {field: [] for field in FIELDS}
    missing: list[str] = []
    for key in keys:
        if key not in left or key not in right:
            missing.append(f"{key}: missing from {'left' if key not in left else 'right'}")
            continue
        for field in FIELDS:
            if left[key][field] != right[key][field]:
                differences[field].append(key)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as out:
        out.write("# Render-plan comparison\n\n")
        out.write(f"- Left: `{args.left}`\n")
        out.write(f"- Right: `{args.right}`\n")
        out.write(f"- Samples: **{len(keys)}**\n")
        out.write(f"- Missing: **{len(missing)}**\n\n")
        out.write("| Field | Identical | Different |\n")
        out.write("|---|---:|---:|\n")
        for field in FIELDS:
            count = len(differences[field])
            out.write(f"| `{field}` | {len(keys) - count - len(missing)} | {count} |\n")
        out.write("\n")
        for field in FIELDS:
            if not differences[field]:
                continue
            out.write(f"## `{field}` differences\n\n")
            out.write("| Asset | Sample | Left | Right |\n")
            out.write("|---|---|---|---|\n")
            for key in differences[field]:
                out.write(
                    f"| `{key[0]}` | `{key[1]}` | `{left[key][field]}` | "
                    f"`{right[key][field]}` |\n"
                )
            out.write("\n")
        if missing:
            out.write("## Missing rows\n\n")
            for item in missing:
                out.write(f"- {item}\n")

    # Keep generated Markdown stable and avoid an empty line at end-of-file.
    normalized = args.output.read_text(encoding="utf-8").rstrip() + "\n"
    args.output.write_text(normalized, encoding="utf-8")

    print(
        "Compared",
        len(keys),
        "plan samples;",
        ", ".join(f"{field}={len(differences[field])}" for field in FIELDS),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
