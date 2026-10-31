#!/usr/bin/env python3
"""The Phase 2 P4 benchmark (PLAN.md): boot the stock TS2068 ROM on the
board's core 0, type the fixed BASIC loop (test/machine/basic_loop.c), and
time each frame with no display, audio or TS-Pico. Then run the same frames
on the host (test/machine/run_ts2068 --bench) and check both end with the
same 48K of RAM.

    python3 tools/p4/bench_ts2068.py [--port /dev/cu.wchusbserial10]
                                     [--frames 2800] [--start 300]

The board needs a MicroPython image with the ts2068 module
(usermods/ts2068) and the ROMs in /roms (tc2068-0.rom, tc2068-1.rom).
"""

import argparse
import ast
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RUN = os.path.join(ROOT, "test", "machine", "run_ts2068")
ROMS = os.path.join(ROOT, "third_party", "fuse-tspico", "roms")
BUDGET_US = 16635                       # one 60.1145 Hz frame

DEVICE = """
import time, ts2068
home = open('/roms/tc2068-0.rom', 'rb').read()
ex = open('/roms/tc2068-1.rom', 'rb').read()
ts2068.bench(home, ex, %d, %d)
del home, ex
while ts2068.status()['state'] != 'done':
    time.sleep_ms(500)
print(repr(ts2068.status()))
"""


def fnv1a(data):
    h = 2166136261
    for c in data:
        h = ((h ^ c) * 16777619) & 0xffffffff
    return h


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.wchusbserial10")
    ap.add_argument("--frames", type=int, default=2800)
    ap.add_argument("--start", type=int, default=300)
    a = ap.parse_args()

    out = subprocess.run(["mpremote", "connect", a.port, "exec",
                          DEVICE % (a.frames, a.start)],
                         check=True, capture_output=True, text=True).stdout
    s = ast.literal_eval(out.strip().splitlines()[-1])

    n = s["frames"] - s["start"]
    mean = s["window_us"] / n
    print("P4, core 0, %s Z80 core: %d frames timed (%d to %d), BASIC loop running"
          % (s.get("z80", "fuse"), n, s["start"], s["frames"]))
    print("  per frame: mean %.0f us, min %d, max %d; budget %d us (%.0f%% used)"
          % (mean, s["min_us"], s["max_us"], BUDGET_US, 100 * mean / BUDGET_US))
    print("  frames over budget: %d; boot frames (0 to %d): mean %.0f us"
          % (s["over_budget"], s["start"], s["boot_us"] / s["start"]))
    print("  histogram, 1 ms buckets: %s" % " ".join(
        "%d-%dms:%d" % (i, i + 1, c) for i, c in enumerate(s["hist"]) if c))
    print("  emulated RAM in internal memory: %s" % s["ram_internal"])

    with tempfile.TemporaryDirectory() as tmp:
        ram = os.path.join(tmp, "ram.bin")
        subprocess.run([RUN, "--rom0", os.path.join(ROMS, "tc2068-0.rom"),
                        "--rom1", os.path.join(ROMS, "tc2068-1.rom"),
                        "--frames", str(s["frames"]), "--bench", "--ram", ram],
                       check=True)
        host = fnv1a(open(ram, "rb").read())
    same = host == s["ram_hash"]
    print("  RAM after %d frames: P4 %08x, host %08x: %s"
          % (s["frames"], s["ram_hash"], host, "same" if same else "DIFFERENT"))
    return 0 if same else 1


if __name__ == "__main__":
    sys.exit(main())
