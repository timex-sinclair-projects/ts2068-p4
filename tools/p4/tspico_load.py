# Run on the board (mpremote run tools/p4/tspico_load.py): boot the TS-Pico
# ROM with the firmware plugged in, type LOAD "" through ts2068.keys(), and
# print the frame, audio and bridge counters whenever they change, for 25 s.
# Needs TSPICO-23.ROM in /TS2068/ROMS on the card (tools/p4/sd_roms.py).

import emu, ts2068, p4bridge, time

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

K = ("frame", "underruns", "late", "pico_waits", "pico_wait_max", "work_max")
print("ms", K, "bridge_frames")
prev = None
t = time.ticks_ms()
while time.ticks_diff(time.ticks_ms(), t) < 25000:
    s = ts2068.run_status()
    v = tuple(s[k] for k in K) + (p4bridge.stats()[0],)
    if prev is None or v[1:] != prev[1:]:
        print(time.ticks_diff(time.ticks_ms(), t), v)
    prev = v
    time.sleep_ms(100)
print("waited on the firmware, us:", ts2068.run_status()["pico_wait_us"])
