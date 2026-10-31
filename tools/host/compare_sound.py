#!/usr/bin/env python3
"""Run a ROM in machine/ (test/machine/run_ts2068 --audio) and in
fuse-tspico (--automation-capture-audio), and compare every sound sample
(PLAN.md Phase 3, "Audio").

    python3 tools/host/compare_sound.py [--fuse PATH] [--frames 300]
                                        [--rom sound|stock|tspico|all]
                                        [--rates 44100,48000]
                                        [--run PATH] [--tolerance N]

ROMs: the sound test (test/basic/sound.asm: the beeper, MIC, and the AY's
tones, noise, envelopes and sample playback), the stock TS2068 ROM and the
TS-Pico ROM, as compare_fuse.py runs them. Fuse is the reference build in
~/esp/ref/fuse-tspico, built where long is 64 bits, so its Blip_Buffer
accuracy is 32 (machine/sound/blipbuffer.h).

--run picks another build of run_ts2068: run_ts2068-float has the P4's
float filters (machine/sound/sound_real.h), which differ from Fuse's double
by a least significant bit now and then, so it's run with --tolerance 1.
"""

import argparse
import array
import os
import subprocess
import sys
import tempfile
import wave

from compare_fuse import ROOT, RUN, roms as fuse_roms


def roms(which, tmp):
    out = []
    if which in ("sound", "all"):
        d = os.path.join(ROOT, "test", "basic")
        out.append(("sound", os.path.join(d, "sound-home.rom"),
                    os.path.join(d, "sound-exrom.rom")))
    if which != "sound":
        out += fuse_roms("all" if which == "all" else which, tmp)
    return out


def run_fuse(fuse, home, ex, frames, rate, outdir):
    os.makedirs(outdir, exist_ok=True)
    subprocess.run([fuse, "--machine", "ts2068", "--rom-ts2068-0", home,
                    "--rom-ts2068-1", ex, "--sound-freq", str(rate),
                    "--automation-output", outdir,
                    "--automation-frames", str(frames),
                    "--automation-capture-audio"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   cwd=os.path.dirname(fuse))
    w = wave.open(os.path.join(outdir, "audio.wav"))
    assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == rate
    return array.array("h", w.readframes(w.getnframes()))


def run_ours(run, home, ex, frames, rate, path):
    subprocess.run([run, "--rom0", home, "--rom1", ex, "--frames", str(frames),
                    "--audio", path, "--rate", str(rate)], check=True)
    return array.array("h", open(path, "rb").read())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fuse", default=os.path.expanduser("~/esp/ref/fuse-tspico/fuse"))
    ap.add_argument("--frames", type=int, default=300)
    ap.add_argument("--rom", default="all", choices=("sound", "stock", "tspico", "all"))
    ap.add_argument("--rates", default="44100,48000")
    ap.add_argument("--run", default=RUN)
    ap.add_argument("--tolerance", type=int, default=0,
                    help="largest difference from Fuse that still matches")
    a = ap.parse_args()

    run = os.path.abspath(a.run)
    if not os.path.exists(run):
        sys.exit("build it first: make -C test/machine")
    print("compare_sound: %s, tolerance %d" % (os.path.basename(run), a.tolerance))
    failed = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, home, ex in roms(a.rom, tmp):
            if name == "timing":
                continue
            for rate in (int(r) for r in a.rates.split(",")):
                d = os.path.join(tmp, "%s-%d" % (name, rate))
                f = run_fuse(a.fuse, home, ex, a.frames, rate, os.path.join(d, "fuse"))
                o = run_ours(run, home, ex, a.frames, rate, os.path.join(d, "ours.raw"))
                diff = [i for i in range(min(len(f), len(o))) if f[i] != o[i]]
                worst = max((abs(f[i] - o[i]) for i in diff), default=0)
                ok = len(f) == len(o) and worst <= a.tolerance
                failed += not ok
                peak = max(map(abs, f)) if f else 0
                line = "%-6s %5d Hz, %d frames: %d samples, peak %d: %s" % (
                    name, rate, a.frames, len(f), peak, "match" if ok else "DIFFER")
                if len(f) != len(o):
                    line += "; fuse %d samples, ours %d" % (len(f), len(o))
                if diff:
                    line += "; %d differ, by up to %d, first at %d (fuse %d ours %d)" % (
                        len(diff), worst, diff[0], f[diff[0]], o[diff[0]])
                print(line, flush=True)
    print("compare_sound: %s" % ("PASS" if not failed else "FAIL (%d)" % failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
