# Run on the board (mpremote run tools/p4/capture_load.py): the fuse-smoke
# session (test/traces) on the live machine. Boots the TS-Pico ROM with the
# firmware plugged in, types LOAD "" through ts2068.keys(), keeps every bus
# frame (ts2068.pico_trace) until none have come for 5 s, and writes them
# to /p4/capture.bin, 3 bytes a frame. Then, on the host:
#
#   mpremote cp :/p4/capture.bin capture.bin
#   python3 tools/host/compare_frames.py --timing capture.bin test/traces/fuse-smoke.frames
#
# The flash and card must hold the trace's seed and nothing the firmware
# has written since (test/traces/seeds/README.md), and the firmware must
# not have run since the last reset.

import emu, ts2068, time

ts2068.pico_trace(300000)
emu.start(rom="TSPICO-23.ROM")
time.sleep(3)


def press(*keys, ms=150):
    """Hold keys, (half-row, bit) each, then let go"""
    rows = bytearray(b"\xff" * 8)
    for h, b in keys:
        rows[h] &= ~(1 << b)
    ts2068.keys(rows)
    time.sleep_ms(ms)
    ts2068.keys()
    time.sleep_ms(150)


J, P, SYM, ENTER = (6, 3), (5, 0), (7, 1), (6, 0)
press(J)                                # LOAD
press(SYM, P)                           # "
press(SYM, P)
press(ENTER)

last, quiet = -1, 0
while quiet < 50:
    time.sleep_ms(100)
    n = len(ts2068.pico_trace())
    quiet = quiet + 1 if n == last else 0
    last = n
data = ts2068.pico_trace()
ts2068.pico_trace(0)
with open("/p4/capture.bin", "wb") as f:
    f.write(data)
print("frames", len(data) // 3)
