#!/usr/bin/env python3
"""Run test/basic/q10probe.asm on the host machine and print its counts;
with --hw, compare a real TS2068's against them (PLAN.md §1.6, Q10).

    make -C test/machine q10probe          # builds it and runs this
    python3 tools/host/q10probe.py [--hw q10res.bin] [--tolerance 2]

The host runs it twice: with the TS2068's contention as this machine has
it (only HOME RAM contended) and with Fuse's (DOCK and EXROM too). Both
use Fuse's 6,5,4,3,2,1,0,0 pattern. q10res.bin is the 256 bytes the probe
leaves at 37376 (SAVE "q10res" CODE 37376,256 on the TS2068). A count
within --tolerance of the model's counts as a match: the interrupt's
timing on real hardware varies by a few T-states, which can move a count
by one.
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
RUN = os.path.join(ROOT, "test", "machine", "run_ts2068")
PROBE = os.path.join(ROOT, "test", "basic", "q10probe.bin")
ROMS = os.path.join(ROOT, "third_party", "fuse-tspico", "roms")
RESULTS = 0x9200
FRAMES = 1700           # boot (300), the tests (about 900), snow (300)

TESTS = ["REF", "R40", "R7F", "W5A", "X5A", "EXR", "DCK", "INFB",
         "80FE", "40FE", "80FF", "40FF", "80F4", "40F4", "40FB", "FRM"]
# Shown as losses against a reference; the rest as counts
REF = {"R40": "REF", "R7F": "REF", "W5A": "REF", "X5A": "REF",
       "EXR": "REF", "DCK": "REF", "80FE": "INFB", "40FE": "INFB",
       "80FF": "INFB", "40FF": "INFB", "80F4": "INFB", "40F4": "INFB",
       "40FB": "INFB"}
PADS = range(11, 19)


def decode(raw):
    v = struct.unpack("<128H", raw[:256])
    return {t: list(v[i * 8:i * 8 + 8]) for i, t in enumerate(TESTS)}


def run(fuse):
    with tempfile.TemporaryDirectory() as d:
        ram = os.path.join(d, "ram.bin")
        cmd = [RUN, "--rom0", os.path.join(ROMS, "tc2068-0.rom"),
               "--rom1", os.path.join(ROMS, "tc2068-1.rom"),
               "--frames", str(FRAMES), "--call", "0x8000", PROBE, "--ram", ram]
        if fuse:
            cmd.append("--fuse-contention")
        subprocess.run(cmd, check=True)
        with open(ram, "rb") as f:
            data = f.read()
    off = RESULTS - 0x4000
    return decode(data[off:off + 256])


def row(r, t):
    if t in REF:
        return [a - b for a, b in zip(r[REF[t]], r[t])]
    return r[t]


def table(title, r):
    print(title)
    print("  %-5s" % "pad" + "".join("%7d" % p for p in PADS))
    for t in TESTS:
        print("  %-5s" % t + "".join("%7d" % x for x in row(r, t)))
    print()


def frame_length(r):
    """T-states a frame, from FRM: 50 frames of passes of P T-states, each
    frame's interrupt (IM 2, 19, and the handler, 65) in between"""
    out = []
    for n, pad in zip(r["FRM"], PADS):
        p = 42 + pad
        out.append((n * p + 50 * 84) / 50.0)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hw", help="a real TS2068's results, 256 bytes")
    ap.add_argument("--tolerance", type=int, default=2)
    a = ap.parse_args()

    if not os.path.exists(PROBE):
        sys.exit("build it first: make -C test/machine q10probe")
    ours, fuse = run(False), run(True)
    table("This machine (only HOME RAM contended):", ours)
    table("Fuse's contention (DOCK and EXROM too):", fuse)
    print("Frame length from FRM, lower bound: " +
          " ".join("%.0f" % f for f in frame_length(ours)))
    differ = [t for t in TESTS if row(ours, t) != row(fuse, t)]
    print("They differ in: " + (", ".join(differ) or "nothing"))
    if not a.hw:
        return 0

    with open(a.hw, "rb") as f:
        hw = decode(f.read())
    print()
    table("The TS2068 (%s):" % a.hw, hw)
    bad = 0
    for name, model in (("this machine", ours), ("Fuse", fuse)):
        off = [t for t in TESTS
               if any(abs(x - y) > a.tolerance for x, y in zip(row(hw, t), row(model, t)))]
        print("Against %s: %s" % (name, "all match" if not off else "differ in " + ", ".join(off)))
        bad += name == "this machine" and bool(off)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
