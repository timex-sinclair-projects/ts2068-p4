#!/usr/bin/env python3
"""Compare the TS-Pico bus frames the P4 recorded from the live machine
(ts2068.pico_trace(), 3 bytes a frame) with a desktop trace (test/traces).

    python3 tools/host/compare_frames.py [--timing] captured.bin test/traces/fuse-smoke.frames

The capture holds every frame. A trace holds a status read only when its
reply differs from the last one logged (pico_host.py's TSPICO_TRACE), and
it starts with Fuse's HELLO, which in-process has no need of, so the
capture is thinned the same way and HELLOs are left out of both. Prints
where the two first part, with some context, and exits 1 if they do.

Two differences are expected between any two runs, and --timing allows
for them:
  - the session id, bytes 3-4 of a 10-byte LOAD/SAVE pre-header (flag 00
    or FF), which the ROM takes from FRAMES (firmware docs/reference,
    glossary, "Session id"), and so the pre-header's XOR, byte 9;
  - which BUSY/READY changes of the status byte the Z80 happened to see:
    with --timing, status reads are left out altogether.
"""

import sys

NAMES = ("OUT0E", "IN0E", "IN0F", "OUT0F", "HELLO")
OPS = {n: i for i, n in enumerate(NAMES)}


def thin(frames):
    out, last = [], None
    for op, v, r in frames:
        if op == 2:
            if r == last:
                continue
            last = r
        out.append((op, v, r))
    return out


def read_capture(path):
    data = open(path, "rb").read()
    return [tuple(data[i:i + 3]) for i in range(0, len(data) - 2, 3)]


def read_trace(path):
    out = []
    for line in open(path):
        if line.strip():
            op, v, _, r = line.split()
            out.append((OPS[op], int(v, 16), int(r, 16)))
    return out


def mask_sessions(frames):
    """Zero the session id and XOR of each pre-header: ten OUT (0Eh) in a
    row, flag 00 or FF, whose XOR checks"""
    out = list(frames)
    for i in range(len(out) - 9):
        run = out[i:i + 10]
        if all(f[0] == 0 for f in run) and run[0][1] in (0x00, 0xFF):
            x = 0
            for f in run[:9]:
                x ^= f[1]
            if x == run[9][1]:
                for j in (3, 4, 9):
                    out[i + j] = (0, 0, 0)
    return out


def show(f):
    return "%s %02X -> %02X" % (NAMES[f[0]], f[1], f[2])


def main():
    args = [a for a in sys.argv[1:] if a != "--timing"]
    if len(args) != 2:
        sys.exit(__doc__)
    ours = [f for f in thin(read_capture(args[0])) if f[0] != 4]
    ref = [f for f in read_trace(args[1]) if f[0] != 4]
    if "--timing" in sys.argv:
        ours = mask_sessions([f for f in ours if f[0] != 2])
        ref = mask_sessions([f for f in ref if f[0] != 2])
    n = min(len(ours), len(ref))
    i = next((i for i in range(n) if ours[i] != ref[i]), n)
    print("P4 %d frames (thinned), trace %d" % (len(ours), len(ref)))
    if i == n and len(ours) == len(ref):
        print("SAME")
        return 0
    print("first difference at frame %d:" % (i + 1))
    for j in range(max(0, i - 5), min(max(len(ours), len(ref)), i + 6)):
        a = show(ours[j]) if j < len(ours) else "-"
        b = show(ref[j]) if j < len(ref) else "-"
        print("%s %6d  %-18s %-18s" % ("*" if a != b else " ", j + 1, a, b))
    return 1


if __name__ == "__main__":
    sys.exit(main())
