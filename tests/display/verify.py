"""Compare the real LVGL output to the RMK status artwork and layout."""
import json
import sys
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = Path(sys.argv[1])
raw = (ROOT / "display/assets/lcd_status_base_68x160.raw").read_bytes()
symbols = json.loads((ROOT / "display/assets/status_symbols.json").read_text())


def stamp(image, pattern, x, y, color=0):
    for row, pixels in enumerate(pattern):
        for column, ink in enumerate(pixels):
            if ink == "#":
                image.putpixel((x + column, y + row), color)


def reference(battery, charging, connected, valid, layer=0, profile=0):
    image = Image.new("L", (68, 160), 255)
    for y in range(160):
        for x in range(68):
            if raw[y * 9 + x // 8] & (1 << (x % 8)):
                image.putpixel((x, y), 0)
    pen = ImageDraw.Draw(image)
    fill = 0 if battery < 0 else (13 * battery + 50) // 100
    if fill:
        pen.rectangle((4, 6, 3 + fill, 10), fill=0)
    if charging:
        for y, row in enumerate(symbols["icons"]["reel_charge_mark"]):
            for x, ink in enumerate(row):
                if ink == "#":
                    image.putpixel((8 + x, 6 + y), 255 if 8 + x < 4 + fill else 0)
    x = 24
    for character in ("--%" if battery < 0 else f"{battery}%"):
        glyph = symbols["glyphs"][character]
        stamp(image, glyph["rows"], x, 4 + glyph["top"])
        x += glyph["advance"]
    for bounds in ((52, 9, 55, 12), (57, 6, 60, 12), (62, 3, 65, 12)):
        pen.rectangle(bounds, fill=0 if connected else None, outline=0)
    if valid:
        stamp(image, symbols["icons"]["reel_layer_pointer"], 4, 65 + layer * 13)
    for index in range(5):
        x = 23 + index * 8
        pen.rectangle((x, 148, x + 4, 152), fill=0 if valid and index == profile else None,
                      outline=0)
    return image


states = [(75, False, True, True, 0, 0), (50, True, True, True, 1, 2),
          (100, False, True, True, 2, 4), (3, True, True, True, 3, 1),
          (0, False, False, False), (-1, False, False, False)]
images = []
for index, state in enumerate(states):
    actual = Image.open(OUTPUT / f"state-{index}.pgm").convert("L")
    expected = reference(*state)
    diff = sum(a != b for a, b in zip(actual.tobytes(), expected.tobytes()))
    if diff:
        expected.save(OUTPUT / f"expected-{index}.png")
        raise AssertionError(f"state {index}: {diff} pixels differ from RMK appearance")
    images.append(actual)
    actual.resize((272, 640), Image.Resampling.NEAREST).save(OUTPUT / f"state-{index}.png")
assert Image.open(OUTPUT / "reconnected.pgm").tobytes() == images[0].tobytes(), "stale reconnect UI"

labels = ["BASE / 75%", "L1 / charging", "L2 / 100%", "AML / charging", "Disconnected", "Unknown"]
sheet = Image.new("RGB", (6 * 304 + 24, 720), "#f1f2f3")
pen = ImageDraw.Draw(sheet)
for index, (label, image) in enumerate(zip(labels, images)):
    x = 24 + index * 304
    pen.text((x, 18), label, fill="#202428")
    sheet.paste(image.resize((272, 640), Image.Resampling.NEAREST).convert("RGB"), (x, 48))
sheet.save(OUTPUT / "reel-lcd-preview.png")
print("Real LVGL render matches RMK artwork in all 6 states; reconnect restores state.")
