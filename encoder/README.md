# ReeL roller

The production configuration uses the LPF-only firmware at 4c76982 (Spike
Filter strength 3, no Response Curve), plus the encoder path confirmed by the
owner on comparison firmware 40a77ac on 2026-11-16 JST. The diagnostic firmware
remains on codex/reel-active-encoder.

## Electrical acquisition

The working topology is P1.14 = common C, P1.15 = phase A, PCB center/GND =
phase B. These roles are inferred from the working active scan, not an
independent continuity measurement of the assembled component.

With both GPIOs pulled up, released C reads the B-C contact; A can be masked
while B is open. The driver drives C low through an open-drain output for 5 us
and reads A, then releases C and waits 8 us. Before/after released readings
must agree. C is never driven high. Phase A stays a pulled-up input.

The target scan period remains 250 us, with 1 ms A debounce and reset of
decoder history after a gap longer than 1 ms. These are the tested comparison
firmware's timing settings; the unused P1.15 probe, other two decoders and LCD
counters are removed. Invalid brackets are skipped; ambiguous transitions
never reuse the previous direction. A dedicated thread sends each confirmed
click as +20/-20 degrees through the existing ZMK sensor/split path, preserving
event order. The existing keymap maps 18 clicks per rotation to vertical scroll,
horizontal scroll on L2, and volume while left GUI is held.

Scanning is still continuous. This cleanup does not introduce an untested
idle/sleep optimization or claim measured battery life. Finite local/BLE queues
can lose input under overload; the driver logs local queue overflow. Very fast
motion can hide intermediate states. The simplified production build needs a
fresh device check after flashing.

## Validation

CI compiles and runs tests/encoder/check.c. It covers contact reconstruction,
the wrong-probe alternating-direction regression, count/direction across
revolutions and reversal, bounce, ambiguous transitions and clock wraparound.
The independent LVGL render tests check the normal display.

Flash all three files from the same successful production build when replacing
the Response Curve diagnostic setup: reel_left.uf2, reel_right.uf2 and
reel_dongle.zip. Keep the dongle ZIP intact for USB DFU. On device, check slow
clicks both ways, immediate reversal at adjacent detents, 18 clicks each way,
normal LCD status, keys, LPF pointer movement and AML.
