#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def load(path: Path) -> dict[tuple[str, str], dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        rows = csv.DictReader(stream, delimiter="\t")
        return {(row["asset"], row["sample"]): row for row in rows}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fail-on-difference", action="store_true")
    args = parser.parse_args()

    left = load(args.left)
    right = load(args.right)
    keys = sorted(set(left) | set(right))
    pixel_differences: list[tuple[tuple[str, str], dict[str, str], dict[str, str]]] = []
    metadata_differences: dict[str, tuple[dict[str, str], dict[str, str]]] = {}
    missing: list[str] = []
    for key in keys:
        if key not in left or key not in right:
            missing.append(f"{key}: missing from {'left' if key not in left else 'right'}")
            continue
        lrow, rrow = left[key], right[key]
        if lrow["frame_fnv64"] != rrow["frame_fnv64"]:
            pixel_differences.append((key, lrow, rrow))
        if any(lrow[field] != rrow[field] for field in
               ("width", "height", "framerate", "duration", "total_frames")):
            metadata_differences[key[0]] = (lrow, rrow)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as stream:
        stream.write("# rlottie reference comparison\n\n")
        stream.write(f"- Left: `{args.left}`\n")
        stream.write(f"- Right: `{args.right}`\n")
        stream.write(f"- Compared normalized samples: **{len(keys)}**\n")
        stream.write(f"- Pixel-identical samples: **{len(keys) - len(pixel_differences) - len(missing)}**\n")
        stream.write(f"- Pixel-different samples: **{len(pixel_differences)}**\n")
        stream.write(f"- Assets with metadata differences: **{len(metadata_differences)}**\n")
        stream.write(f"- Missing samples: **{len(missing)}**\n\n")

        if metadata_differences:
            stream.write("## Metadata differences\n\n")
            stream.write("| Asset | Left frames/duration | Right frames/duration |\n")
            stream.write("|---|---|---|\n")
            for asset, (lrow, rrow) in sorted(metadata_differences.items()):
                stream.write(
                    f"| `{asset}` | {lrow['total_frames']} / {lrow['duration']} | "
                    f"{rrow['total_frames']} / {rrow['duration']} |\n")
            stream.write("\n")

        if pixel_differences:
            stream.write("## Pixel differences at normalized samples\n\n")
            stream.write("| Asset | Sample | Left frame/hash | Right frame/hash |\n")
            stream.write("|---|---|---|---|\n")
            for (asset, sample), lrow, rrow in pixel_differences:
                stream.write(
                    f"| `{asset}` | `{sample}` | {lrow['frame']} / `{lrow['frame_fnv64']}` | "
                    f"{rrow['frame']} / `{rrow['frame_fnv64']}` |\n")
            stream.write("\n")
        if missing:
            stream.write("## Missing samples\n\n")
            for item in missing:
                stream.write(f"- {item}\n")

    print(
        f"Compared {len(keys)} samples: {len(pixel_differences)} pixel differences, "
        f"{len(metadata_differences)} metadata differences, {len(missing)} missing")
    return 1 if args.fail_on_difference and (pixel_differences or metadata_differences or missing) else 0


if __name__ == "__main__":
    raise SystemExit(main())
