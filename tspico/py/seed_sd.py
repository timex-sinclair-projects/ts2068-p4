# Make the board's microSD card exactly what a trace started from
# (test/traces/seeds): FORMAT it (FAT), then copy /seed/sd/* onto it and make
# the empty folders the seed lists. tools/p4/install.py --trace puts /seed
# on the flash.
#
#   import seed_sd; seed_sd.run()           # erases the card
#
# Formatting on the board, not on a computer, keeps the card free of the
# files a desktop OS adds (.fseventsd, System Volume Information, ._*),
# which the firmware would list.

import os
import machine
import json


def _copy(src, dst):
    if os.stat(src)[0] & 0x4000:
        try:
            os.mkdir(dst)
        except OSError:
            pass
        for n in os.listdir(src):
            _copy(src + "/" + n, dst + "/" + n)
    else:
        buf = bytearray(4096)
        with open(src, "rb") as fi, open(dst, "wb") as fo:
            while True:
                n = fi.readinto(buf)
                if not n:
                    break
                fo.write(buf[:n] if n < len(buf) else buf)


def run():
    with open("/p4/sd.json") as f:
        a = json.load(f)
    a["data"] = tuple(a["data"])
    sd = machine.SDCard(**a)
    print("[seed_sd] card:", sd.info())
    os.VfsFat.mkfs(sd)
    os.mount(sd, "/sd")
    try:
        _copy("/seed/sd", "/sd")
        try:
            with open("/seed/empty_dirs.txt") as f:
                for line in f:
                    p = line.strip()
                    if p:
                        os.mkdir("/sd/" + p)
        except OSError:
            pass
        print("[seed_sd] /sd:", os.listdir("/sd"))
    finally:
        os.umount("/sd")
    sd.deinit()
