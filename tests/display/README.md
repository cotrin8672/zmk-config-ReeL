# ReeL display checks

The test executable compiles the production `reel_canvas.c` and generated assets
against the exact LVGL revision pinned by ZMK. It compiles and executes the real
Zephyr mono display adapter at `10ba6d0cb38bc3d258775d27982f707599320085`, then captures
the bytes passed to `display_write()`. The capture uses ls0xx's actual MONO01,
LSB-first, 1=white format. Rotating those physical pixels back to portrait checks
the RMK artwork and status geometry, with the unused Bluetooth footer removed.
It covers all four layers, charging at low/medium battery levels,
0/100/unknown battery, disconnect, and reconnected rendering.

LVGL uses a custom **fixed 8192-byte arena**, matching the device display pool's
capacity rather than unlimited libc malloc. The allocator has 16-byte host
alignment/headers and first-fit allocation with coalescing, so its bookkeeping
cost is conservative compared with the 32-bit firmware. Any failed allocation
fails the test, and 120 further state changes must preserve the initial frame.
Peak pool usage and the largest failed request are printed.

The previous 68x160 A1 base required a 11523-byte temporary allocation when LVGL
expanded its 9-byte packed rows to 72-byte A8 rows. It therefore disappeared on
the 8192-byte device heap while the dynamic widgets remained visible. With the
old generated assets selected through `REEL_TEST_ASSET_SOURCE`, the bounded
test reproduces the missing layer labels, battery outline and separator. The
runtime base is now a native A8 alpha image in flash, used directly by LVGL
without that temporary bitmap. The original raw source remains unchanged.

This checks the LVGL rendering path, including glyph metrics, color polarity,
rotation, palette skipping, nonzero-row partial flushes, and clearing of stale
marks. Disabling the stock nice_view inversion must reproduce the exact
complementary image; this guards against testing only the pre-adapter LVGL pixels.
The Zephyr header shims provide only host-side device/capability declarations;
pixel conversion is unchanged upstream code. The test does not simulate
Bluetooth delivery, battery measurement or the physical LCD.

## Run

Requirements: CMake, a C/C++ compiler, LVGL at
`f1db87ee98f1810328a8419572fa42a3b5f352ae`, Zephyr at
`10ba6d0cb38bc3d258775d27982f707599320085`, and Python 3 with Pillow.
Use a build/output directory outside the source checkout:

```powershell
rtk proxy python tools/generate-display-assets.py --check
rtk proxy cmake -S tests/display -B C:/path/to/build -DLVGL_DIR=C:/path/to/lvgl -DZEPHYR_DIR=C:/path/to/zephyr
rtk proxy cmake --build C:/path/to/build --target reel_display_preview --parallel
# Create C:/path/to/render before running the executable.
rtk proxy C:/path/to/build/reel_display_preview.exe C:/path/to/render
rtk proxy python tests/display/verify.py C:/path/to/render
```

On Linux/macOS the executable has no `.exe` suffix. The verifier writes enlarged
state PNGs and `reel-lcd-preview.png` after the comparisons pass. CI runs the same
checks separately from the three Zephyr firmware builds.
