#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow is not installed; contact sheet skipped")
        return 0

    files = sorted(args.frames.glob("*.ppm"))
    if not files:
        return 0
    thumbs = []
    for path in files:
        image = Image.open(path).convert("RGB")
        image.thumbnail((128, 128))
        thumbs.append((path.name, image.copy()))
    columns = 5
    cell_w, cell_h = 220, 170
    rows = (len(thumbs) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), "white")
    draw = ImageDraw.Draw(sheet)
    for index, (name, image) in enumerate(thumbs):
        x = (index % columns) * cell_w
        y = (index // columns) * cell_h
        sheet.paste(image, (x + 4, y + 4))
        draw.text((x + 4, y + 136), name[:34], fill="black")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
