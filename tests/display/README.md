# ReeL display checks

The test executable compiles the production `reel_canvas.c` and generated assets
against the exact LVGL revision pinned by ZMK. It renders through an I1 display
flush callback, rotates the actual panel pixels back to portrait, then compares
every pixel against the RMK artwork and status geometry. It covers all four
layers, multiple profiles, charging at low/medium battery levels, 0/100/unknown
battery, disconnect, and reconnected rendering.

This checks the LVGL rendering path, including glyph metrics, color polarity,
rotation and clearing of stale marks. It does not simulate Bluetooth delivery,
battery measurement or the physical LCD.

## Run

Requirements: CMake, a C/C++ compiler, LVGL at
`f1db87ee98f1810328a8419572fa42a3b5f352ae`, and Python 3 with Pillow.
Use a build/output directory outside the source checkout:

```powershell
rtk proxy python tools/generate-display-assets.py --check
rtk proxy cmake -S tests/display -B C:/path/to/build -DLVGL_DIR=C:/path/to/lvgl
rtk proxy cmake --build C:/path/to/build --target reel_display_preview --parallel
# Create C:/path/to/render before running the executable.
rtk proxy C:/path/to/build/reel_display_preview.exe C:/path/to/render
rtk proxy python tests/display/verify.py C:/path/to/render
```

On Linux/macOS the executable has no `.exe` suffix. The verifier writes enlarged
state PNGs and `reel-lcd-preview.png` after the comparisons pass. CI runs the same
checks separately from the three Zephyr firmware builds.
