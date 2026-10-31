#!/usr/bin/env python3
"""Check SZX snapshots (machine/szx.c) against libspectrum and fuse-tspico
(PLAN.md Phase 3, "Storage").

    python3 tools/host/compare_szx.py [--fuse PATH] [--at 32,160,320] [--then 100]
                                      [--rom sound|stock|tspico|all]

For each ROM and each frame count N in --at, with M = --then:

  1. ours runs N frames and saves a.szx;
  2. libspectrum (szx_resave) reads it and writes it back uncompressed:
     it must be the same file, byte for byte; and compressed, c.szx, as
     Fuse writes snapshots;
  3. ours loads c.szx (inflating it) and runs M frames;
  4. Fuse loads a.szx and runs M frames;
  5. ours runs N + M frames straight through.

The CPU state and the screen of 3, 4 and 5 must all match, and the sound
of 3 and 4 (both start their sound at the load). N should be a multiple
of 32 frames: the FLASH phase isn't in a snapshot, and both restart it.
Screens are compared as compare_fuse.py compares them.

Fuse's R: fuse-tspico's automation reports R as (z80.r & 0x7f) | z80.r7
(automation/state.c) without masking r7 to bit 7, and a snapshot load sets
r7 to the whole saved R byte (z80_from_snapshot: R = R7 = r). The core
itself masks r7 when it reads R, so only the report is wrong. Our R is
put through the same formula, with the saved byte, before it's compared
with Fuse's.
"""

import argparse
import array
import filecmp
import json
import os
import subprocess
import sys
import tempfile
import wave

from PIL import Image

from compare_fuse import ROOT, RUN, CPU_KEYS, screen_diff
from compare_sound import roms

RESAVE = os.path.join(ROOT, "test", "machine", "szx_resave")
RATE = 44100


def ours(home, ex, frames, d, load=None, save=None):
    os.makedirs(d, exist_ok=True)
    cmd = [RUN, "--rom0", home, "--rom1", ex, "--frames", str(frames),
           "--fuse-contention", "--state", os.path.join(d, "state.json"),
           "--screen", os.path.join(d, "screen.ppm"),
           "--audio", os.path.join(d, "audio.raw"), "--rate", str(RATE)]
    if load:
        cmd += ["--load-szx", load]
    if save:
        cmd += ["--save-szx", save]
    subprocess.run(cmd, check=True)
    with open(os.path.join(d, "state.json")) as f:
        cpu = json.load(f)["cpu"]
    audio = array.array("h", open(os.path.join(d, "audio.raw"), "rb").read())
    return cpu, Image.open(os.path.join(d, "screen.ppm")).convert("RGB"), audio


def fuse(path, home, ex, snap, frames, d):
    """Two runs: the screen with no sound, then the sound with no screen.
    Fuse's Timex PNG writer reads past its buffer (compare_fuse.py), which
    crashes it about half the time when sound is on, as the memory then
    lies differently."""
    base = [path, "--machine", "ts2068", "--rom-ts2068-0", home,
            "--rom-ts2068-1", ex, "--snapshot", snap,
            "--automation-frames", str(frames)]
    screen, sound = os.path.join(d, "screen"), os.path.join(d, "sound")
    for out, extra in ((screen, ["--no-sound", "--automation-capture-screen"]),
                       (sound, ["--sound-freq", str(RATE), "--automation-capture-audio"])):
        os.makedirs(out, exist_ok=True)
        subprocess.run(base + ["--automation-output", out] + extra, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       cwd=os.path.dirname(path))
    d = screen
    with open(os.path.join(d, "result.json")) as f:
        cpu = json.load(f)["state"]["cpu"]
    w = wave.open(os.path.join(sound, "audio.wav"))
    audio = array.array("h", w.readframes(w.getnframes()))
    return cpu, Image.open(os.path.join(d, "screen.png")).convert("RGB"), audio


def saved_r(snap):
    d = open(snap, "rb").read()
    i = d.index(b"Z80R")
    return d[i + 8 + 25]


def same_screen(a, b):
    """Two of our 640 x 240 screens"""
    return a.tobytes() == b.tobytes()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fuse", default=os.path.expanduser("~/esp/ref/fuse-tspico/fuse"))
    ap.add_argument("--at", default="32,160,320")
    ap.add_argument("--then", type=int, default=100)
    ap.add_argument("--rom", default="all", choices=("sound", "stock", "tspico", "all"))
    a = ap.parse_args()

    for p in (RUN, RESAVE):
        if not os.path.exists(p):
            sys.exit("build it first: make -C test/machine szx")
    failed = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, home, ex in roms(a.rom, tmp):
            if name == "timing":
                continue
            for n in (int(x) for x in a.at.split(",")):
                d = os.path.join(tmp, "%s-%d" % (name, n))
                os.makedirs(d)
                snap, back, packed = (os.path.join(d, x) for x in ("a.szx", "b.szx", "c.szx"))
                ours(home, ex, n, os.path.join(d, "save"), save=snap)
                subprocess.run([RESAVE, snap, back], check=True)
                subprocess.run([RESAVE, snap, packed, "--compress"], check=True)
                problems = []
                if not filecmp.cmp(snap, back, shallow=False):
                    problems.append("libspectrum writes it differently")
                lc, ls, la = ours(home, ex, a.then, os.path.join(d, "load"), load=packed)
                fc, fs, fa = fuse(a.fuse, home, ex, snap, a.then, os.path.join(d, "fuse"))
                sc, ss, _ = ours(home, ex, n + a.then, os.path.join(d, "straight"))
                as_fuse = dict(lc, r=(lc["r"] & 0x7f) | saved_r(snap))
                for what, c, o in (("Fuse", fc, as_fuse), ("straight through", sc, lc)):
                    regs = [k for k in CPU_KEYS if c[k] != o[k]]
                    if regs:
                        problems.append("registers vs %s: " % what + ", ".join(
                            "%s %d ours %d" % (k, c[k], o[k]) for k in regs))
                bad, first = screen_diff(ls, fs)
                if bad:
                    problems.append("%d screen pixels differ from Fuse, first at %s" % (bad, first))
                if not same_screen(ls, ss):
                    problems.append("screen differs from straight through")
                if fa != la:
                    diff = [i for i in range(min(len(fa), len(la))) if fa[i] != la[i]]
                    problems.append("sound: fuse %d samples, ours %d, %d differ%s" % (
                        len(fa), len(la), len(diff),
                        ", first at %d" % diff[0] if diff else ""))
                failed += bool(problems)
                print("%-6s at %4d, then %d: %s (%d bytes, %d compressed)" % (
                    name, n, a.then, "; ".join(problems) or "match",
                    os.path.getsize(snap), os.path.getsize(packed)), flush=True)
    print("compare_szx: %s" % ("PASS" if not failed else "FAIL (%d)" % failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
