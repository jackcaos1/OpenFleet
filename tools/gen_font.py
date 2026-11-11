#!/usr/bin/env python3
"""Erzeugt engine/src/core/FontData.h: Atlas einer freien Monospace-Schrift (DejaVu Sans Mono, Bitstream-Vera-Lizenz, frei einbettbar).
Aufruf: python3 -I tools/gen_font.py [pfad/zur/DejaVuSansMono.ttf]   (nur noetig, wenn Schrift/Zeichen geaendert werden)"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

TTF = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
SIZE = 18
CELL_W, CELL_H = 11, 22
COLS, ROWS = 16, 8
# Zeichen: ASCII 32..126, danach diese Zusatzzeichen (Unicode-Codepunkte)
EXTRA = [0xE4, 0xF6, 0xFC, 0xC4, 0xD6, 0xDC, 0xDF, 0xB0, 0x2019, 0x2013, 0x2014, 0x2026, 0x20AC, 0xD7, 0xB7, 0xB1, 0xE9, 0xE8, 0xE0, 0xE2]
CODES = list(range(32, 127)) + EXTRA

font = ImageFont.truetype(TTF, SIZE)
img = Image.new("L", (COLS * CELL_W, ROWS * CELL_H), 0)
d = ImageDraw.Draw(img)
for i, c in enumerate(CODES):
    x, y = (i % COLS) * CELL_W, (i // COLS) * CELL_H
    d.text((x, y + 1), chr(c), fill=255, font=font)
data = list(img.tobytes())
out = []
out.append("// Automatisch erzeugt von tools/gen_font.py (DejaVu Sans Mono, frei einbettbar). Nicht von Hand aendern.")
out.append("#pragma once\n#include <cstdint>\nnamespace fleet {")
out.append("constexpr int kFontCellW = %d, kFontCellH = %d, kFontCols = %d, kFontRows = %d;" % (CELL_W, CELL_H, COLS, ROWS))
out.append("constexpr int kFontExtraCount = %d;" % len(EXTRA))
out.append("constexpr uint16_t kFontExtra[kFontExtraCount] = {%s};" % ",".join(str(c) for c in EXTRA))
out.append("constexpr uint8_t kFontAtlas[%d] = {" % len(data))
for i in range(0, len(data), 32):
    out.append(",".join(str(v) for v in data[i:i + 32]) + ",")
out.append("};\n}  // namespace fleet")
path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "engine", "src", "core", "FontData.h")
open(path, "w").write("\n".join(out) + "\n")
img.save("/tmp/font_atlas.png")
print("Atlas", img.size, "Zeichen", len(CODES))
