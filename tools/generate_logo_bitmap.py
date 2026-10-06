#!/usr/bin/env python3
"""Rasterize the simple M/L/Z pear SVG into a monochrome Arduino bitmap."""

from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw


WIDTH = 144
HEIGHT = 200
OVERSAMPLE = 4


def parse_subpaths(path_data: str) -> list[list[tuple[float, float]]]:
    tokens = re.findall(r"[MLZmlz]|-?\d+(?:\.\d+)?", path_data)
    subpaths: list[list[tuple[float, float]]] = []
    current: list[tuple[float, float]] = []
    index = 0

    while index < len(tokens):
        token = tokens[index]
        if token.upper() in {"M", "L"}:
            if token.upper() == "M" and current:
                subpaths.append(current)
                current = []
            current.append((float(tokens[index + 1]), float(tokens[index + 2])))
            index += 3
        elif token.upper() == "Z":
            if current:
                subpaths.append(current)
                current = []
            index += 1
        else:
            raise ValueError(f"Unsupported SVG path token: {token}")

    if current:
        subpaths.append(current)
    return subpaths


def rasterize(svg_path: Path) -> Image.Image:
    root = ET.parse(svg_path).getroot()
    paths = [
        parse_subpaths(element.attrib["d"])
        for element in root.findall("{http://www.w3.org/2000/svg}path")
    ]
    points = [point for path in paths for subpath in path for point in subpath]
    min_x = min(point[0] for point in points)
    max_x = max(point[0] for point in points)
    min_y = min(point[1] for point in points)
    max_y = max(point[1] for point in points)

    target_width = (WIDTH - 4) * OVERSAMPLE
    target_height = (HEIGHT - 4) * OVERSAMPLE
    scale = min(target_width / (max_x - min_x), target_height / (max_y - min_y))
    offset_x = (WIDTH * OVERSAMPLE - (max_x - min_x) * scale) / 2
    offset_y = (HEIGHT * OVERSAMPLE - (max_y - min_y) * scale) / 2

    combined = Image.new("1", (WIDTH * OVERSAMPLE, HEIGHT * OVERSAMPLE))
    for path in paths:
        path_mask = Image.new("1", combined.size)
        for subpath in path:
            polygon = [
                (
                    round(offset_x + (x - min_x) * scale),
                    round(offset_y + (y - min_y) * scale),
                )
                for x, y in subpath
            ]
            subpath_mask = Image.new("1", combined.size)
            ImageDraw.Draw(subpath_mask).polygon(polygon, fill=1)
            path_mask = ImageChops.logical_xor(path_mask, subpath_mask)
        combined = ImageChops.lighter(combined, path_mask)

    antialiased = combined.convert("L").resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    return antialiased.point(lambda value: 255 if value >= 96 else 0, mode="1")


def write_header(bitmap: Image.Image, output_path: Path) -> None:
    packed: list[int] = []
    for y in range(HEIGHT):
        for byte_x in range((WIDTH + 7) // 8):
            value = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < WIDTH and bitmap.getpixel((x, y)):
                    value |= 0x80 >> bit
            packed.append(value)

    rows = [
        "  " + ", ".join(f"0x{value:02X}" for value in packed[index:index + 16]) + ","
        for index in range(0, len(packed), 16)
    ]
    output_path.write_text(
        "#pragma once\n\n"
        "#include <Arduino.h>\n\n"
        f"constexpr int kPearLogoWidth = {WIDTH};\n"
        f"constexpr int kPearLogoHeight = {HEIGHT};\n"
        "constexpr uint8_t kPearLogoBitmap[] PROGMEM = {\n"
        + "\n".join(rows)
        + "\n};\n",
        encoding="utf-8",
    )


def write_preview(bitmap: Image.Image, output_path: Path) -> None:
    preview = Image.new("RGB", (240, 240), "black")
    white_logo = Image.new("RGB", bitmap.size, "white")
    preview.paste(white_logo, ((240 - WIDTH) // 2, (240 - HEIGHT) // 2), bitmap.convert("L"))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    preview.save(output_path)


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: generate_logo_bitmap.py INPUT.svg OUTPUT.h PREVIEW.png")
    bitmap = rasterize(Path(sys.argv[1]))
    write_header(bitmap, Path(sys.argv[2]))
    write_preview(bitmap, Path(sys.argv[3]))


if __name__ == "__main__":
    main()
