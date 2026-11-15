#!/usr/bin/env python3
"""Generate native LVGL 9 display assets; see display/assets/README.md.

Generation and --check use only the Python standard library. Pillow is needed
only for the optional --preview PNG. No RMK checkout is needed to regenerate.
"""

import argparse
import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "display" / "assets"
WIDTH, HEIGHT = 68, 160
ROW_BYTES = (WIDTH + 7) // 8
BASE_SOURCE = "lcd_status_base_68x160.raw"
SYMBOL_SOURCE = "status_symbols.json"


def bitmap(rows):
    """Read human-editable monochrome rows: '#' is opaque ink, '.' is clear."""
    if not rows or not rows[0] or any(len(row) != len(rows[0]) for row in rows):
        raise ValueError("bitmap rows must be nonempty and have equal widths")
    if any(set(row) - {"#", "."} for row in rows):
        raise ValueError("bitmap rows may contain only '#' and '.'")
    return [[int(pixel == "#") for pixel in row] for row in rows]


def pack_msb(rows, *, row_aligned):
    """Images have byte-aligned rows; LV_FONT_FMT_TXT_PLAIN does not."""
    bits = []
    for row in rows:
        bits.extend(row)
        if row_aligned:
            bits.extend([0] * (-len(row) % 8))
    bits.extend([0] * (-len(bits) % 8))
    return bytes(sum(bits[pos + bit] << (7 - bit) for bit in range(8))
                 for pos in range(0, len(bits), 8))


def load_sources():
    raw = (ASSETS / BASE_SOURCE).read_bytes()
    if len(raw) != ROW_BYTES * HEIGHT:
        raise ValueError(f"{BASE_SOURCE}: expected {ROW_BYTES * HEIGHT} bytes")
    if any(raw[y * ROW_BYTES + ROW_BYTES - 1] & 0xF0 for y in range(HEIGHT)):
        raise ValueError(f"{BASE_SOURCE}: nonzero unused row padding bits")
    base = [[(raw[y * ROW_BYTES + x // 8] >> (x % 8)) & 1
             for x in range(WIDTH)] for y in range(HEIGHT)]
    symbol_bytes = (ASSETS / SYMBOL_SOURCE).read_bytes()
    symbols = json.loads(symbol_bytes)
    if symbols["line_height"] != 9:
        raise ValueError("the original status font must have line_height 9")
    if set(symbols["glyphs"]) != set("0123456789%-"):
        raise ValueError("the status font must contain exactly ASCII 0-9, '%' and '-'")
    for character, glyph in symbols["glyphs"].items():
        rows = bitmap(glyph["rows"])
        width, height = len(rows[0]), len(rows)
        expected = (5, 9, 6, 0) if character.isdigit() else (
            (8, 8, 8, 0) if character == "%" else (5, 1, 6, 4))
        if (width, height, glyph["advance"], glyph["top"]) != expected:
            raise ValueError(f"{character!r}: invalid original glyph metrics")
    icon_sizes = {"reel_charge_mark": (5, 5), "reel_layer_pointer": (4, 5)}
    if symbols["icons"].keys() != icon_sizes.keys():
        raise ValueError("expected the charge mark and layer pointer icons")
    for name, size in icon_sizes.items():
        rows = bitmap(symbols["icons"][name])
        if (len(rows[0]), len(rows)) != size:
            raise ValueError(f"{name}: expected size {size}")
    hashes = {BASE_SOURCE: hashlib.sha256(raw).hexdigest(),
              SYMBOL_SOURCE: hashlib.sha256(symbol_bytes).hexdigest()}
    return base, symbols, hashes


def c_bytes(data):
    return "\n".join("    " + ", ".join(f"0x{byte:02x}" for byte in data[pos:pos + 12]) + ","
                     for pos in range(0, len(data), 12))


def image_c(name, rows):
    width, height = len(rows[0]), len(rows)
    return f"""static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {name}_map[] = {{
{c_bytes(pack_msb(rows, row_aligned=True))}
}};

const lv_image_dsc_t {name} = {{
    .header = {{
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_A1,
        .flags = 0,
        .w = {width},
        .h = {height},
        .stride = {(width + 7) // 8},
        .reserved_2 = 0,
    }},
    .data_size = sizeof({name}_map),
    .data = {name}_map,
    .reserved = NULL,
}};
"""


def font_c(symbols):
    packed = bytearray()
    descriptors = ["    {0}, /* Reserved glyph ID 0. */"]
    for character in sorted(symbols["glyphs"]):
        glyph = symbols["glyphs"][character]
        rows = bitmap(glyph["rows"])
        width, height = len(rows[0]), len(rows)
        offset = symbols["line_height"] - height - glyph["top"]
        descriptors.append(
            f"    {{.bitmap_index = {len(packed)}, .adv_w = {glyph['advance'] * 16}, "
            f".box_w = {width}, .box_h = {height}, .ofs_x = 0, .ofs_y = {offset}}}, "
            f"/* {character} */")
        packed.extend(pack_msb(rows, row_aligned=False))
    return """/* Font bitmaps are continuous MSB-first pixels within each glyph.
 * Only the end of a glyph is byte padded (LV_FONT_FMT_TXT_PLAIN). */
static const LV_ATTRIBUTE_LARGE_CONST uint8_t reel_status_glyph_bitmap[] = {
""" + c_bytes(packed) + """
};

static const lv_font_fmt_txt_glyph_dsc_t reel_status_glyph_dsc[] = {
""" + "\n".join(descriptors) + """
};

static const lv_font_fmt_txt_cmap_t reel_status_cmaps[] = {
    {.range_start = 37, .range_length = 1, .glyph_id_start = 1,
     .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}, /* % */
    {.range_start = 45, .range_length = 1, .glyph_id_start = 2,
     .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}, /* - */
    {.range_start = 48, .range_length = 10, .glyph_id_start = 3,
     .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}, /* 0-9 */
};

static const lv_font_fmt_txt_dsc_t reel_status_font_dsc = {
    .glyph_bitmap = reel_status_glyph_bitmap,
    .glyph_dsc = reel_status_glyph_dsc,
    .cmaps = reel_status_cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 3,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = LV_FONT_FMT_TXT_PLAIN,
};

const lv_font_t reel_status_font = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .release_glyph = NULL,
    .line_height = REEL_STATUS_FONT_LINE_HEIGHT,
    .base_line = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .kerning = LV_FONT_KERNING_NONE,
    .underline_position = 0,
    .underline_thickness = 0,
    .dsc = &reel_status_font_dsc,
    .fallback = NULL,
    .user_data = NULL,
};
"""


def generate(base, symbols, hashes):
    provenance = "/* Generated by tools/generate-display-assets.py; do not edit.\n"
    provenance += " * RMK artwork provenance: display/assets/README.md.\n"
    for name, digest in hashes.items():
        provenance += f" * {name} SHA-256: {digest}\n"
    provenance += " */\n"
    header = provenance + """
#ifndef REEL_ASSETS_H
#define REEL_ASSETS_H

#include <lvgl.h>

#if LVGL_VERSION_MAJOR != 9
#error "ReeL display assets require LVGL 9."
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define REEL_STATUS_BASE_WIDTH 68
#define REEL_STATUS_BASE_HEIGHT 160
#define REEL_STATUS_FONT_LINE_HEIGHT 9
#define REEL_STATUS_FONT_ADVANCE 6

/* Portrait A1 masks: MSB first, byte-aligned rows, bit 1 = opaque ink.
 * Draw in black on a white background; no palette or driver is included. */
extern const lv_image_dsc_t reel_status_base;
extern const lv_image_dsc_t reel_charge_mark;
extern const lv_image_dsc_t reel_layer_pointer;

/* ASCII 0-9, '%' and '-'; usable as lv_draw_label_dsc_t.font.
 * Digit/dash advance 6, percent advance 8; use letter_space = 0.
 * Digits and percent start at line top; dash ink is at top + 4. */
extern const lv_font_t reel_status_font;

#ifdef __cplusplus
}
#endif

#endif /* REEL_ASSETS_H */
"""
    source = provenance + '\n#include "reel_assets.h"\n\n'
    source += image_c("reel_status_base", base) + "\n"
    for name in sorted(symbols["icons"]):
        source += image_c(name, bitmap(symbols["icons"][name])) + "\n"
    source += font_c(symbols)
    return {ROOT / "display" / "reel_assets.h": header.encode("utf-8"),
            ROOT / "display" / "reel_assets.c": source.encode("utf-8")}


def preview(path, base, symbols):
    from PIL import Image, ImageDraw

    def stamp(target, rows, x, y):
        for dy, row in enumerate(rows):
            for dx, ink in enumerate(row):
                if ink:
                    target.putpixel((x + dx, y + dy), 0)

    def text(target, value, x, y):
        for character in value:
            glyph = symbols["glyphs"][character]
            stamp(target, bitmap(glyph["rows"]), x, y + glyph["top"])
            x += glyph["advance"]

    scale = 4
    canvas = Image.new("L", (4 * (WIDTH * scale + 16), HEIGHT * scale + 108), 238)
    labels = ImageDraw.Draw(canvas)
    for index, value in enumerate((None, "0%", "100%", "--%")):
        image = Image.new("L", (WIDTH, HEIGHT), 255)
        stamp(image, base, 0, 0)
        if value is not None:
            text(image, value, 24, 4)
        left = 8 + index * (WIDTH * scale + 16)
        labels.text((left, 4), "Static base" if value is None else value, fill=0)
        canvas.paste(image.resize((WIDTH * scale, HEIGHT * scale), Image.Resampling.NEAREST),
                     (left, 24))
    strip = Image.new("L", (150, 16), 255)
    text(strip, "0123456789%--", 2, 2)
    stamp(strip, bitmap(symbols["icons"]["reel_charge_mark"]), 102, 2)
    stamp(strip, bitmap(symbols["icons"]["reel_layer_pointer"]), 114, 2)
    canvas.paste(strip.resize((600, 64), Image.Resampling.NEAREST), (8, HEIGHT * scale + 36))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path, format="PNG")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="fail if C/header are stale; write nothing")
    mode.add_argument("--preview", type=Path, metavar="PNG", help="also write a PNG preview (requires Pillow)")
    args = parser.parse_args()
    try:
        base, symbols, hashes = load_sources()
        outputs = generate(base, symbols, hashes)
        stale = [path for path, data in outputs.items()
                 if not path.is_file() or path.read_bytes() != data]
        if args.check:
            if stale:
                for path in stale:
                    print(f"Stale or missing: {path.relative_to(ROOT).as_posix()}", file=sys.stderr)
                return 1
            print("Display assets match their sources (3 A1 images, 12 font glyphs).")
            return 0
        if args.preview:
            preview(args.preview, base, symbols)
        for path in stale:
            path.write_bytes(outputs[path])  # Fixed UTF-8/LF bytes, including on Windows.
        print(f"Display assets generated ({len(stale)} files updated).")
        if args.preview:
            print(f"Preview: {args.preview.resolve()}")
        return 0
    except (OSError, ValueError, KeyError, TypeError, ImportError) as error:
        parser.exit(1, f"Display asset generation failed: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
