"""Check the RMK-to-ZMK XIAO boot layout before publishing either image."""
from pathlib import Path
from struct import unpack_from

for side in ("left", "right"):
    image = Path(f"reel_{side}.uf2").read_bytes()
    assert image and len(image) % 512 == 0
    vector = None
    for pos in range(0, len(image), 512):
        magic0, magic1, flags, addr, size, _, _, family = unpack_from("<8I", image, pos)
        assert (magic0, magic1, family) == (0x0A324655, 0x9E5D5157, 0xADA52840)
        assert flags & 0x2000 and 0 < size <= 476 and 0x1000 <= addr < addr + size <= 0xD4000
        if addr == 0x1000:
            vector = unpack_from("<II", image, pos + 32)
    assert vector, f"{side}: missing XIAO vector table at RMK boot address"
    stack, reset = vector
    assert 0x20000000 < stack <= 0x20040000 and reset & 1 and 0x1000 <= reset < 0xD4000
