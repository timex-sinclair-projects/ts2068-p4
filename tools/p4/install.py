#!/usr/bin/env python3
"""Put the TS-Pico firmware and our launcher on the P4's flash filesystem.

    python3 tools/p4/install.py [--trace fuse-smoke] [--port PORT]

Stages everything in a temporary folder, then copies it with mpremote:

  /TS/*.mpy          the pinned firmware (firmware/src/TS), compiled by
                     ~/esp/micropython's mpy-cross (same bytecode version)
  /dev_extcmd.py     as pico_host has it on its path (identical to TS/extcmd)
  /config.ini        TELEMETRY on, as pico_host's build_root sets it
  /words.txt, /assets/*.tap   firmware/src (run firmware/tools/build-basic.sh first)
  /p4host.py, /p4replay.py    tspico/py
  /p4/sd.json        the board's microSD (tools/p4/board.json)
  /p4/build.txt      the firmware commit, as pico_host shows it
  /traces/*.bin      test/traces/*.frames, packed

--trace overlays that trace's seed flash files (test/traces/seeds/<name>/flash)
and stages its card contents in /seed, which seed_sd.py (on the board) writes
to a freshly formatted microSD.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FW = os.path.join(REPO, "firmware")
MPY_CROSS = os.path.expanduser("~/esp/micropython/mpy-cross/build/mpy-cross")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pack_frames                                              # noqa: E402

# test/traces/seeds/README.md: what each trace's card held besides its sd/ folder.
SD_HELP = ("zesarux-smoke",)                    # firmware/SD card/help -> /sd/help
EMPTY_DIRS = {"zesarux-smoke": ("TAP/GAMES",)}   # git can't hold an empty folder


def git(*args):
    return subprocess.run(["git", "-C", FW] + list(args), capture_output=True, text=True).stdout.strip()


def stage(trace):
    st = tempfile.mkdtemp(prefix="p4stage-")
    src = os.path.join(FW, "src")
    os.makedirs(os.path.join(st, "TS"))
    for name in sorted(os.listdir(os.path.join(src, "TS"))):
        if name.endswith(".py"):
            out = os.path.join(st, "TS", name[:-3] + ".mpy")
            subprocess.run([MPY_CROSS, "-o", out, "-s", "TS/" + name,
                            os.path.join(src, "TS", name)], check=True)
    shutil.copy(os.path.join(src, "dev_extcmd.py"), st)
    shutil.copy(os.path.join(src, "words.txt"), st)
    with open(os.path.join(src, "config.ini")) as f:
        cfg = json.load(f)
    cfg["TELEMETRY"] = True
    with open(os.path.join(st, "config.ini"), "w") as f:
        json.dump(cfg, f)
    if not os.path.exists(os.path.join(src, "assets", "nofile.tap")):
        sys.exit("firmware/src/assets has no TAPs: run firmware/tools/build-basic.sh first")
    shutil.copytree(os.path.join(src, "assets"), os.path.join(st, "assets"))
    if trace:
        seed = os.path.join(REPO, "test", "traces", "seeds", trace)
        if os.path.isdir(os.path.join(seed, "flash")):
            shutil.copytree(os.path.join(seed, "flash"), st, dirs_exist_ok=True)
        # The card's contents, for seed_sd.py to copy on the board (test/traces/seeds/README.md).
        sd = os.path.join(st, "seed", "sd")
        os.makedirs(sd)
        if trace in SD_HELP:
            shutil.copytree(os.path.join(FW, "SD card", "help"), os.path.join(sd, "help"))
        if os.path.isdir(os.path.join(seed, "sd")):
            shutil.copytree(os.path.join(seed, "sd"), sd, dirs_exist_ok=True)
        with open(os.path.join(st, "seed", "empty_dirs.txt"), "w") as f:
            f.write("".join(d + "\n" for d in EMPTY_DIRS.get(trace, ())))
    for name in ("p4host.py", "p4replay.py", "seed_sd.py"):
        p = os.path.join(REPO, "tspico", "py", name)
        if os.path.exists(p):
            shutil.copy(p, st)
    os.makedirs(os.path.join(st, "p4"))
    with open(os.path.join(REPO, "tools", "p4", "board.json")) as f:
        board = json.load(f)
    with open(os.path.join(st, "p4", "sd.json"), "w") as f:
        json.dump(board["sd"], f)
    with open(os.path.join(st, "p4", "build.txt"), "w") as f:
        f.write("%s (%s)\n" % (git("rev-parse", "--short", "HEAD"), git("rev-parse", "--abbrev-ref", "HEAD")))
    os.makedirs(os.path.join(st, "traces"))
    tdir = os.path.join(REPO, "test", "traces")
    for name in sorted(os.listdir(tdir)):
        if name.endswith(".frames"):
            with open(os.path.join(tdir, name)) as f:
                data = pack_frames.pack(f.readlines())
            with open(os.path.join(st, "traces", name[:-7] + ".bin"), "wb") as f:
                f.write(data)
    return st, board


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trace", help="overlay this trace's seed flash files")
    ap.add_argument("--port")
    ap.add_argument("--stage-only", action="store_true")
    args = ap.parse_args()
    st, board = stage(args.trace)
    print("staged in", st)
    if args.stage_only:
        return
    port = args.port or board["port"]
    mp = ["mpremote", "connect", port]
    # Old firmware files first: a stale .py next to a new .mpy would win the import.
    subprocess.run(mp + ["exec", "import os\n"
                         "def rm(p):\n"
                         "  try:\n"
                         "    if os.stat(p)[0] & 0x4000:\n"
                         "      for n in os.listdir(p): rm(p+'/'+n)\n"
                         "      os.rmdir(p)\n"
                         "    else: os.remove(p)\n"
                         "  except OSError: pass\n"
                         "for p in ('/TS','/assets','/traces','/p4','/TMP','/seed'): rm(p)\n"], check=True)
    subprocess.run(mp + ["fs", "cp", "-r"] + [os.path.join(st, n) for n in sorted(os.listdir(st))] + [":"],
                   check=True)
    subprocess.run(mp + ["fs", "ls", ":"], check=True)
    shutil.rmtree(st)


if __name__ == "__main__":
    main()
