#!/usr/bin/env python3
"""Boot a ROM in machine/ (test/machine/run_ts2068) and in fuse-tspico, and
compare the CPU state and the screen after each of several frame counts
(PLAN.md Phase 2, "Host check").

    python3 tools/host/compare_fuse.py [--fuse PATH] [--frames 1,50,300]
                                       [--rom stock|tspico|timing|all] [--ts2068]

ROMs: the stock TS2068 ROM, the TS-Pico ROM (firmware/src/rom/TSPICO-23.ROM,
split), and the DOCK/EXROM contention test (test/basic/dockhome.asm, whose
counts are printed after the last frame count).

Fuse is the reference build in ~/esp/ref/fuse-tspico (docs/NOTES.md,
"Reference build"), run headless through its automation options with no
TS-Pico attached. Our machine runs with Fuse's DOCK/EXROM contention unless
--ts2068 is given, which shows what the TS2068's own contention changes.

Screens: ours is 640 x 240 (machine_render). Fuse's PNG (1280 x 960) is
garbled on Timex machines: its automation writes the PNG through Fuse's
screenshot code with the "Double size" scaler instead of from the 640 x 480
buffer it captured, and each PNG row's left 640 pixels hold only the left
half of a line (the right 640 repeat it two rows down). So only our
x 0-319, the left border and the screen's left 16 columns, is compared,
at Fuse's (2x, 4y). docs/NOTES.md, "Phase 2 progress".
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RUN = os.path.join(ROOT, "test", "machine", "run_ts2068")   # + "-jls" for --core jls
FUSE_ROMS = os.path.join(ROOT, "third_party", "fuse-tspico", "roms")
CPU_KEYS = ("af", "bc", "de", "hl", "af_alt", "bc_alt", "de_alt", "hl_alt",
            "ix", "iy", "sp", "pc", "i", "r", "iff1", "iff2",
            "interrupt_mode", "halted", "frame_tstate")


def roms(which, tmp):
    """(name, home path, exrom path) for each ROM set asked for."""
    out = []
    if which in ("stock", "all"):
        out.append(("stock", os.path.join(FUSE_ROMS, "tc2068-0.rom"),
                    os.path.join(FUSE_ROMS, "tc2068-1.rom")))
    if which in ("tspico", "all"):
        src = os.path.join(ROOT, "firmware", "src", "rom", "TSPICO-23.ROM")
        data = open(src, "rb").read()
        assert len(data) == 0x8000, src
        home, ex = os.path.join(tmp, "tspico-home.rom"), os.path.join(tmp, "tspico-exrom.rom")
        open(home, "wb").write(data[:0x4000])
        open(ex, "wb").write(data[0x4000:])
        out.append(("tspico", home, ex))
    if which in ("timing", "all"):
        d = os.path.join(ROOT, "test", "basic")
        home, ex = os.path.join(d, "dockhome-home.rom"), os.path.join(d, "dockhome-exrom.rom")
        if not os.path.exists(home):
            sys.exit("build the timing ROM first: make -C test/machine")
        out.append(("timing", home, ex))
    return out


# test/basic/dockhome.asm: which register holds each test's count
TIMING = (("bc", "read  0x4000 HOME"), ("de", "read  0x4000 EXROM"),
          ("hl", "read  0x4000 DOCK"), ("bc_alt", "read  0x8000 HOME"),
          ("de_alt", "run at 0x4000 HOME"), ("hl_alt", "run at 0x4000 EXROM"),
          ("ix", "IN 40FBh HOME"), ("iy", "IN 40FBh EXROM"))


# The ROM has finished all eight tests by this frame
TIMING_FRAMES = 25


def timing_expected(fuse_cpu, our_cpu, regs):
    """With --ts2068: Fuse and we differ exactly where DOCK/EXROM contention
    matters (tests 2, 3, 6, 8, and so R and the T-state), and those run at
    the uncontended speed."""
    counts = [our_cpu[reg] for reg, _ in TIMING]
    expected = {"de", "hl", "hl_alt", "iy", "r", "frame_tstate"}
    return (set(regs) <= expected and
            counts[1] == counts[2] == counts[3] == counts[5] and
            counts[0] < counts[3] and counts[4] < counts[3] and
            counts[7] > counts[6])


def timing_table(fuse_cpu, our_cpu):
    print("         test                 fuse   ours  (loop passes per frame)")
    for i, (reg, what) in enumerate(TIMING, 1):
        print("         %d %-20s %5d  %5d" % (i, what, fuse_cpu[reg], our_cpu[reg]))


def run_fuse(fuse, home, ex, frames, outdir):
    os.makedirs(outdir, exist_ok=True)
    subprocess.run([fuse, "--machine", "ts2068", "--rom-ts2068-0", home,
                    "--rom-ts2068-1", ex, "--no-sound",
                    "--automation-output", outdir,
                    "--automation-frames", str(frames),
                    "--automation-capture-screen"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   cwd=os.path.dirname(fuse))
    with open(os.path.join(outdir, "result.json")) as f:
        r = json.load(f)
    return r["state"]["cpu"], Image.open(os.path.join(outdir, "screen.png")).convert("RGB")


def run_ours(run, home, ex, frames, outdir, ts2068):
    os.makedirs(outdir, exist_ok=True)
    state, screen = os.path.join(outdir, "state.json"), os.path.join(outdir, "screen.ppm")
    cmd = [run, "--rom0", home, "--rom1", ex, "--frames", str(frames),
           "--state", state, "--screen", screen]
    if not ts2068:
        cmd.append("--fuse-contention")
    subprocess.run(cmd, check=True)
    with open(state) as f:
        r = json.load(f)
    return r["cpu"], Image.open(screen).convert("RGB")


def screen_diff(ours, fuse):
    """Our pixels in x 0-319 that differ from Fuse's, as (count, first (x, y))."""
    assert ours.size == (640, 240) and fuse.size == (1280, 960), (ours.size, fuse.size)
    o, f = ours.load(), fuse.load()
    bad, first = 0, None
    for y in range(240):
        for x in range(320):
            if o[x, y] != f[2 * x, 4 * y]:
                bad += 1
                first = first or (x, y)
    return bad, first


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fuse", default=os.path.expanduser("~/esp/ref/fuse-tspico/fuse"))
    ap.add_argument("--frames", default="1,2,10,50,100,300,1000,3000")
    ap.add_argument("--rom", default="all", choices=("stock", "tspico", "timing", "all"))
    ap.add_argument("--core", default="fuse", choices=("fuse", "jls"),
                    help="which Z80 core our run_ts2068 was built with")
    ap.add_argument("--ts2068", action="store_true",
                    help="our TS2068 contention (DOCK/EXROM never contended); "
                         "the timing ROM must then differ from Fuse as expected")
    a = ap.parse_args()

    run = RUN + ("-jls" if a.core == "jls" else "")
    if not os.path.exists(run):
        sys.exit("build it first: make -C test/machine CORE=%s" % a.core)
    frames = [int(n) for n in a.frames.split(",")]
    failed = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, home, ex in roms(a.rom, tmp):
            for n in frames:
                d = os.path.join(tmp, "%s-%d" % (name, n))
                fc, fs = run_fuse(a.fuse, home, ex, n, os.path.join(d, "fuse"))
                oc, os_ = run_ours(run, home, ex, n, os.path.join(d, "ours"), a.ts2068)
                regs = [k for k in CPU_KEYS if fc[k] != oc[k]]
                bad, first = screen_diff(os_, fs)
                ok = not regs and not bad
                verdict = "match"
                if name == "timing" and a.ts2068 and n >= TIMING_FRAMES:
                    ok, regs = timing_expected(fc, oc, regs), []
                    verdict = "differs from Fuse as expected"
                failed += not ok
                line = "%-6s %5d frames: %s" % (name, n, verdict if ok else "DIFFER")
                if regs:
                    line += "; registers " + ", ".join(
                        "%s fuse %d ours %d" % (k, fc[k], oc[k]) for k in regs)
                if bad:
                    line += "; %d screen pixels differ, first at %s" % (bad, first)
                print(line, flush=True)
                if name == "timing" and n == frames[-1]:
                    timing_table(fc, oc)
    print("compare_fuse: %s" % ("PASS" if not failed else "FAIL (%d)" % failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
