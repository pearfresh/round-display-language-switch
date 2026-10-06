#!/usr/bin/env python3
"""Generate a packed, one-bit firmware QR bitmap for the installer URL."""

from __future__ import annotations

import argparse
from pathlib import Path

import qrcode
from qrcode.constants import ERROR_CORRECT_M


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("url")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    qr = qrcode.QRCode(
        version=None,
        error_correction=ERROR_CORRECT_M,
        box_size=1,
        border=0,
    )
    qr.add_data(args.url)
    qr.make(fit=True)
    matrix = qr.get_matrix()
    size = len(matrix)
    row_bytes = (size + 7) // 8
    packed = bytearray(row_bytes * size)
    for y, row in enumerate(matrix):
        for x, black in enumerate(row):
            if black:
                packed[y * row_bytes + x // 8] |= 0x80 >> (x % 8)

    lines = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        f"constexpr uint8_t kInstallQrSize = {size};",
        f"constexpr uint8_t kInstallQrRowBytes = {row_bytes};",
        f'constexpr char kInstallUrl[] = "{args.url}";',
        "constexpr uint8_t kInstallQrBitmap[] PROGMEM = {",
    ]
    for offset in range(0, len(packed), 12):
        values = ", ".join(f"0x{value:02X}" for value in packed[offset : offset + 12])
        lines.append(f"    {values},")
    lines.extend(
        [
            "};",
            "",
            "inline bool installQrModuleIsBlack(uint8_t x, uint8_t y) {",
            "  const size_t offset = static_cast<size_t>(y) * kInstallQrRowBytes + x / 8;",
            "  return (pgm_read_byte(kInstallQrBitmap + offset) & (0x80 >> (x % 8))) != 0;",
            "}",
            "",
        ]
    )
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"generated {size}x{size} QR for {args.url}")


if __name__ == "__main__":
    main()
