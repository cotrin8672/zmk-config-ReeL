# ReeL status artwork

These are logical portrait assets (68 x 160), derived from the original RMK pixels.
There is no display transport, framebuffer driver, rotation, or status renderer
in this asset package.

## Provenance

Source repository: [cotrin8672/rmk-config-ReeL](https://github.com/cotrin8672/rmk-config-ReeL),
authored by cotrin8672. The source commits and paths were inspected:

- `lcd_status_base_68x160.raw` is a byte-for-byte copy of
  [`src/lcd_status_base_68x160.raw` at 398d72b8d0b50498d6c59bc4adde41159d085d5e](https://github.com/cotrin8672/rmk-config-ReeL/blob/398d72b8d0b50498d6c59bc4adde41159d085d5e/src/lcd_status_base_68x160.raw),
  the commit that introduced the file. It is 1440 bytes: 160 rows of 9 bytes,
  least significant bit first; 1 means black ink. Only the first 68 pixels
  of each row are visible; the remaining four bits are zero.
  SHA-256: `749c175a3d62127e1f599920b70085d4656913794f41212dc698d3e26e64657d`.
- `status_symbols.json` transcribes the pixel patterns in
  [`src/sharp_lcd.rs` at 5ce200072e4ec15dcfaa235fd1f76ef660ccf6e1](https://github.com/cotrin8672/rmk-config-ReeL/blob/5ce200072e4ec15dcfaa235fd1f76ef660ccf6e1/src/sharp_lcd.rs):
  the 5 x 9 `DIGITS` introduced by that commit, the 8 x 8 `PERCENT`,
  the 5 x 5 charging `MARK`, and the active-layer pointer coordinates.
  `#` means ink and `.` means transparent. Digits advance by 6 pixels;
  percent is the original 8-pixel-wide pattern, placed immediately after
  the final digit's six-pixel advance.
- The missing-battery `--` uses `embedded_graphics::mono_font::ascii::FONT_6X10`
  with `Baseline::Top`, as selected by RMK commit
  [`ac682b9cf8a00eceafe7a6752a75aa84f683eb4f`](https://github.com/cotrin8672/rmk-config-ReeL/commit/ac682b9cf8a00eceafe7a6752a75aa84f683eb4f).
  The dash was extracted from the `embedded-graphics` 0.8.2 ASCII raw font,
  matching RMK's `Cargo.lock`: five ink pixels at x=0..4, y=4 in a
  6 x 10 cell, with advance 6. Its empty rows are cropped in the generated
  font while its position is preserved. That crate's `fonts/src/6x10.bdf`
  identifies the font as "Public domain terminal emulator font. Share and enjoy."
  Its `fonts/src/README.md` attributes the source fonts to X.Org.

No separate artwork license notice was found alongside the RMK raw artwork;
this conversion records its origin without asserting a new artwork license.

## Runtime base for USB-dongle use

The raw provenance asset above remains byte-for-byte unchanged. The generator
clears rows **141 through 159 inclusive** in `reel_status_base`: the separator
at y=141 and Bluetooth icon at y=146..154 are omitted. All pixels in rows
0..140, the 68 x 160 dimensions, glyphs, and other icon assets are preserved.
The five profile squares were dynamic geometry, not part of the raw artwork;
the USB-dongle reference screen omits them as well.

Generated comments record both source fingerprints and the derived A1 base's
SHA-256, together with the exact footer-clearing rule.

## Regeneration

Run from the repository root (Python 3, no packages needed):

```powershell
rtk python tools/generate-display-assets.py
rtk python tools/generate-display-assets.py --check
```

`--check` is read-only and exits 1 for missing or different generated files.
Generated files have fixed UTF-8/LF bytes, stable ordering, source SHA-256
comments, and no timestamps or absolute paths. An adjacent RMK checkout is
not required. To create an optional preview outside the source tree, use a
Python environment with Pillow:

```powershell
rtk python tools/generate-display-assets.py --preview C:/Users/gummy/Downloads/reel-assets.png
```

The preview shows the derived runtime base, text placement examples, every glyph,
and both icons. Text examples intentionally add only the font to the base;
they are not a complete status renderer.

## Native LVGL 9 API

Generated `display/reel_assets.h` exports:

| Symbol | Type | Dimensions / metrics |
| --- | --- | --- |
| `reel_status_base` | `const lv_image_dsc_t` | 68 x 160, stride 9 |
| `reel_charge_mark` | `const lv_image_dsc_t` | 5 x 5, stride 1 |
| `reel_layer_pointer` | `const lv_image_dsc_t` | 4 x 5, stride 1 |
| `reel_status_font` | `const lv_font_t` | ASCII `0-9`, `%`, `-`; line height 9, baseline 0 |

The descriptors target ZMK's pinned
[`zmkfirmware/lvgl` f1db87ee98f1810328a8419572fa42a3b5f352ae](https://github.com/zmkfirmware/lvgl/tree/f1db87ee98f1810328a8419572fa42a3b5f352ae).
Images use `LV_IMAGE_HEADER_MAGIC` and `LV_COLOR_FORMAT_A1`, with MSB-first,
byte-aligned rows, zero padding, and no palette. A 1 bit is opaque ink;
set the image draw descriptor's `recolor` to black, `recolor_opa` to
`LV_OPA_COVER`, and draw on white. White recoloring on black is also supported.
The base remains logical portrait; the renderer owns canvas rotation.

For `lv_draw_label`, set `lv_draw_label_dsc_t.font = &reel_status_font`,
`letter_space = 0`, and black text. Digits and percent start at the label
rectangle's top; dash ink starts four pixels below it. Digits and dash
advance 6 pixels, percent advances 8. Thus a label at (24, 4) reproduces
RMK's `0%`, `42%`, `100%`, and `--%` placement exactly. The font has no
kerning, fallback, scaling, or antialiasing. Its native
`LV_FONT_FMT_TXT_PLAIN` bitmaps pack across rows within each glyph, unlike
the byte-aligned A1 images.

Place `reel_charge_mark` at (8, 6) and `reel_layer_pointer` at
(4, 65 + 13 * layer) to recover their original positions. Recoloring the
charging mask white on filled battery pixels remains a renderer concern.
