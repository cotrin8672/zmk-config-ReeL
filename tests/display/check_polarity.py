"""Confirm disabling nice_view inversion reproduces the exact reversed wire image."""
import sys
from pathlib import Path
from PIL import Image

positive, negative = map(Path, sys.argv[1:])
for name in [*(f"state-{i}.pgm" for i in range(6)), "reconnected.pgm"]:
    a, b = (Image.open(folder / name).convert("L") for folder in (positive, negative))
    assert a.size == b.size == (68, 160)
    assert all(x ^ y == 255 for x, y in zip(a.tobytes(), b.tobytes())), name
print("Disabled nice_view inversion reproduces the original reversed LCD polarity in every state.")
