#!/usr/bin/env python3
"""Write platform/p4_osd_font.h: the side-panel menu's typeface, Aileron
Regular (Pillow's built-in font, CC0), as 4-bit anti-aliased bitmaps of
ASCII 32-126 at two sizes, for the short rows and the tall ones.

    uv run --with pillow tools/host/make_osd_font.py
"""

import os

from PIL import Image, ImageDraw, ImageFont

SIZES = (("small", 16), ("large", 24))
FIRST, LAST = 32, 126
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "..", "platform", "p4_osd_font.h")


def build(name, size):
    font = ImageFont.load_default(size=size)
    ascent, descent = font.getmetrics()
    data = bytearray()
    glyphs = []
    for code in range(FIRST, LAST + 1):
        ch = chr(code)
        advance = round(font.getlength(ch))
        im = Image.new("L", (advance + size, ascent + descent))
        ImageDraw.Draw(im).text((size // 2, ascent), ch, font=font,
                                fill=255, anchor="ls")
        box = im.getbbox()
        if box is None:
            glyphs.append((advance, 0, 0, 0, 0, len(data)))
            continue
        x0, y0, x1, y1 = box
        w, h = x1 - x0, y1 - y0
        offset = len(data)
        for y in range(y0, y1):
            row = [im.getpixel((x, y)) * 15 // 255 for x in range(x0, x1)]
            row += [0] * (len(row) & 1)
            for i in range(0, len(row), 2):
                data.append(row[i] | (row[i + 1] << 4))
        glyphs.append((advance, w, h, x0 - size // 2, y0, offset))
    lines = ["static const uint8_t font_%s_data[] = {" % name]
    for i in range(0, len(data), 16):
        lines.append("  " + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ",")
    lines.append("};")
    lines.append("static const p4_osd_glyph_t font_%s_glyphs[] = {" % name)
    for code, g in zip(range(FIRST, LAST + 1), glyphs):
        lines.append("  { %d, %d, %d, %d, %d, %d },  /* %r */" % (g + (chr(code),)))
    lines.append("};")
    lines.append("static const p4_osd_font_t font_%s = {" % name)
    lines.append("  %d, %d, font_%s_glyphs, font_%s_data," % (ascent + descent, ascent, name, name))
    lines.append("};")
    return lines, len(data)


def main():
    out = ["/* p4_osd_font.h: written by tools/host/make_osd_font.py; don't edit.",
           "",
           "   Aileron Regular (Sora Sagano, CC0 1.0), Pillow's built-in font, at",
           "   %s pixels: ASCII %d-%d, 4 bits of coverage a pixel, two pixels a" %
           (" and ".join(str(s) for _, s in SIZES), FIRST, LAST),
           "   byte, low nibble first, rows padded to a whole byte. */",
           "",
           "#ifndef P4_OSD_FONT_H",
           "#define P4_OSD_FONT_H",
           "",
           "#include <stdint.h>",
           "",
           "typedef struct {",
           "  uint8_t advance;             /* to the next glyph's origin */",
           "  uint8_t w, h;                /* the bitmap */",
           "  int8_t x;                    /* the bitmap's left, from the origin */",
           "  uint8_t y;                   /* its top, from the line's top */",
           "  uint16_t offset;             /* into the data */",
           "} p4_osd_glyph_t;",
           "",
           "typedef struct {",
           "  uint8_t height, ascent;",
           "  const p4_osd_glyph_t *glyphs;   /* from ' ' */",
           "  const uint8_t *data;",
           "} p4_osd_font_t;",
           "",
           "#define P4_OSD_FONT_FIRST %d" % FIRST,
           "#define P4_OSD_FONT_LAST  %d" % LAST,
           ""]
    for name, size in SIZES:
        lines, n = build(name, size)
        out += lines + [""]
        print("%s: %d px, %d bytes" % (name, size, n))
    out.append("#endif /* P4_OSD_FONT_H */")
    with open(OUT, "w") as f:
        f.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
