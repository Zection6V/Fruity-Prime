#!/usr/bin/env python3
"""Compile the *entire* k8x12L BDF font to compact read-only C++ bitmap data.

The source is vendored, unmodified in Mods/ClassicMenu/Fonts/k8x12L.bdf.
No per-translation subsets and no hand-maintained Unicode-to-glyph tables.

Source: k8x12L BDF (2021-05-05), Copyright (C) 2015-2021 Num Kadoma.
https://littlelimit.net/k8x12.htm (unrestricted use, copying, modification,
and redistribution; provided as-is without warranty).

Usage: generate-classicmenu-bitmap-font.py --bdf INPUT.bdf --output OUTPUT.hpp
"""
import argparse
import re
import sys
from pathlib import Path

WIDTH = 8
HEIGHT = 12
ASCENT = 10
DESCENT = 2


def compile_bdf(path):
    source = path.read_text(encoding="ascii")
    if not source.startswith("STARTFONT 2.1\n"):
        raise ValueError("unexpected BDF version")
    if "FONTBOUNDINGBOX 8 12 0 -2" not in source:
        raise ValueError("k8x12L dimensions changed")
    for key, expected in (("FONT_ASCENT", ASCENT), ("FONT_DESCENT", DESCENT)):
        match = re.search(r"^" + key + r" (\d+)$", source, re.MULTILINE)
        if match is None or int(match.group(1)) != expected:
            raise ValueError("unexpected " + key)

    count = re.search(r"^CHARS (\d+)$", source, re.MULTILINE)
    if count is None:
        raise ValueError("missing CHARS declaration")
    expected_count = int(count.group(1))
    glyphs = {}
    blocks = re.findall(r"^STARTCHAR[^\n]*\n(.*?)^ENDCHAR[ \t]*$",
                        source, re.MULTILINE | re.DOTALL)
    if len(blocks) != expected_count:
        raise ValueError(f"CHARS says {expected_count}, found {len(blocks)}")
    for block in blocks:
        encoding = re.search(r"^ENCODING (-?\d+)$", block, re.MULTILINE)
        bbox = re.search(r"^BBX (\d+) (\d+) (-?\d+) (-?\d+)$",
                         block, re.MULTILINE)
        if encoding is None or bbox is None:
            raise ValueError("glyph missing ENCODING or BBX")
        codepoint = int(encoding.group(1))
        if codepoint < 0 or codepoint > 0x10FFFF or 0xD800 <= codepoint <= 0xDFFF:
            raise ValueError(f"unsupported codepoint: {codepoint}")
        if codepoint in glyphs:
            raise ValueError(f"duplicate codepoint: U+{codepoint:04X}")
        width, height, offset_x, offset_y = map(int, bbox.groups())
        if not (0 <= offset_x and offset_x + width <= WIDTH
                and -DESCENT <= offset_y and offset_y + height <= ASCENT):
            raise ValueError(f"glyph outside 8x12 box: U+{codepoint:04X}")
        top = ASCENT - (offset_y + height)
        lines = block.splitlines()
        try:
            bitmap_at = lines.index("BITMAP")
        except ValueError as error:
            raise ValueError(f"glyph has no BITMAP: U+{codepoint:04X}") from error
        pixels = lines[bitmap_at + 1:]
        if len(pixels) != height:
            raise ValueError(f"wrong bitmap height: U+{codepoint:04X}")
        rows = [0] * HEIGHT
        for y, hex_bits in enumerate(pixels):
            if not hex_bits or not re.fullmatch(r"[0-9A-Fa-f]+", hex_bits):
                raise ValueError(f"invalid bitmap row: U+{codepoint:04X}")
            padded_bits = len(hex_bits) * 4
            if padded_bits < width:
                raise ValueError(f"bitmap row too narrow: U+{codepoint:04X}")
            bits = int(hex_bits, 16)
            for x in range(width):
                if bits & (1 << (padded_bits - x - 1)):
                    rows[top + y] |= 0x80 >> (offset_x + x)
        glyphs[codepoint] = rows
    if len(glyphs) != expected_count:
        raise ValueError("some glyph encodings were lost")
    return sorted(glyphs.items())


def cpp_header(glyphs):
    records = "\n".join(
        "        {0x%04XU, {{%s}}}," %
        (codepoint, ", ".join(f"0x{value:02X}" for value in rows))
        for codepoint, rows in glyphs)
    header = """// AUTO-GENERATED from Fonts/k8x12L.bdf; DO NOT EDIT.
// k8x12L (2021-05-05), Copyright (C) 2015-2021 Num Kadoma.
// https://littlelimit.net/k8x12.htm ; free to use/modify/redistribute.
// Provided AS IS without warranty. All BDF glyphs are included.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace MphRead::Mods::ClassicMenu
{
    struct K8x12LGlyph final
    {
        char32_t Codepoint;
        std::array<std::uint8_t, 12> Rows;
    };
    static_assert(sizeof(K8x12LGlyph) == 16);

"""
    header += f"    inline constexpr std::array<K8x12LGlyph, {len(glyphs)}> K8x12LGlyphs{{{{\n"
    header += records + "\n    }};\n\n"
    header += """    [[nodiscard]] inline constexpr const K8x12LGlyph* FindK8x12LGlyph(char32_t cp) noexcept
    {
        std::size_t left = 0, right = K8x12LGlyphs.size();
        while (left < right)
        {
            const std::size_t mid = left + (right - left) / 2;
            if (K8x12LGlyphs[mid].Codepoint < cp) left = mid + 1;
            else right = mid;
        }
        return left < K8x12LGlyphs.size() && K8x12LGlyphs[left].Codepoint == cp
            ? &K8x12LGlyphs[left] : nullptr;
    }
}
"""
    return header


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bdf", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        glyphs = compile_bdf(args.bdf)
        generated = cpp_header(glyphs)
        if args.check:
            if not args.output.exists() or args.output.read_text(encoding="utf-8") != generated:
                raise ValueError("generated k8x12L header is out of date")
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            if not args.output.exists() or args.output.read_text(encoding="utf-8") != generated:
                args.output.write_text(generated, encoding="utf-8", newline="\n")
        print(f"k8x12L: compiled {len(glyphs)} glyphs, {len(glyphs)*16} bytes of static data")
        return 0
    except (OSError, ValueError, UnicodeError) as error:
        print(f"k8x12L font compilation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
