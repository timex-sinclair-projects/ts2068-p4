#!/usr/bin/env python3
"""Put TS2068 ROMs on the P4's microSD card, in /TS2068/ROMS, for emu.py
(usermods/ts2068/emu.py).

    python3 tools/p4/sd_roms.py [--port PORT] [FILE ...]

With no files: the stock ROM pair (Fuse's tc2068-0.rom and tc2068-1.rom,
as tools/host/compare_fuse.py uses them) and the TS-Pico ROM
(firmware/src/rom/TSPICO-23.ROM, 32K: HOME then EXROM). The card is
mounted through mpremote for the copy and unmounted after; nothing else
on it is touched. settings.json is left alone: emu.py's defaults apply
until emu.save() writes one.
"""

import argparse
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FUSE_ROMS = os.path.expanduser("~/esp/ref/fuse-tspico/roms")
DEFAULT = [os.path.join(FUSE_ROMS, "tc2068-0.rom"), os.path.join(FUSE_ROMS, "tc2068-1.rom"),
           os.path.join(REPO, "firmware", "src", "rom", "TSPICO-23.ROM")]

MOUNT = """import json, machine, os
a = json.load(open('/p4/sd.json')); a['data'] = tuple(a['data'])
card = machine.SDCard(**a)
try: os.mount(card, '/sd')
except OSError: pass                    # still mounted from a failed run
for d in ('/sd/TS2068', '/sd/TS2068/ROMS'):
    try: os.mkdir(d)
    except OSError: pass
"""
UNMOUNT = "print(os.listdir('/sd/TS2068/ROMS')); os.umount('/sd'); card.deinit()"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="*")
    ap.add_argument("--port")
    a = ap.parse_args()
    with open(os.path.join(REPO, "tools", "p4", "board.json")) as f:
        port = a.port or json.load(f)["port"]
    files = a.files or DEFAULT
    for p in files:
        if os.path.getsize(p) not in (0x2000, 0x4000, 0x8000):
            sys.exit("%s: not 8K, 16K or 32K" % p)
    # One mpremote session, its commands joined by "+", so the mount lasts
    # through the copies
    cmd = ["mpremote", "connect", port, "exec", MOUNT]
    for p in files:
        cmd += ["+", "fs", "cp", p, ":/sd/TS2068/ROMS/" + os.path.basename(p)]
    cmd += ["+", "exec", UNMOUNT]
    subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()
