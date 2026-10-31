#!/usr/bin/env python3
"""Make the release downloads: the whole flash as one image, and the
microSD card's contents, so the board can be set up without building.

    uv run --with littlefs-python --with esptool tools/p4/make_release.py \\
        [--build ~/esp/micropython/ports/esp32/build-CROWPANEL_P4_7] [--out dist]
        [--version v0.1.0]

Build the firmware first (README, "Running it"). VERSION is git describe's
unless given. Writes, in --out:

  ts2068-p4-VERSION.zip      ts2068-p4-VERSION.bin: the 16 MB flash, for
                             esptool write-flash 0x0: the bootloader, the
                             partition table, MicroPython with our modules,
                             and the filesystem install.py would leave
                             (littlefs, as MicroPython makes it)
  ts2068-p4-sd-VERSION.zip   the card: the TS-Pico's card files, and
                             /TS2068 with the ROMs and settings.json
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import zipfile

import littlefs

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import install                                                  # noqa: E402

REPO = install.REPO
BUILD = os.path.expanduser("~/esp/micropython/ports/esp32/build-CROWPANEL_P4_7")

# boards/CROWPANEL_P4_7/partitions.csv: the app ends at 0x410000, and
# MicroPython's vfs partition runs from there to the end of the 16 MB.
VFS_OFFSET, FLASH_SIZE, BLOCK = 0x410000, 0x1000000, 4096
# extmod/vfs_lfsx.c's defaults, which inisetup.py's VfsLfs2.mkfs uses;
# lib/littlefs's on-disk version in MicroPython v1.29
LFS = dict(block_size=BLOCK, block_count=(FLASH_SIZE - VFS_OFFSET) // BLOCK,
           read_size=32, prog_size=32, lookahead_size=32, block_cycles=100,
           name_max=255, disk_version=0x00020001)

ROMS = [os.path.join(REPO, "third_party", "fuse-tspico", "roms", "tc2068-0.rom"),
        os.path.join(REPO, "third_party", "fuse-tspico", "roms", "tc2068-1.rom"),
        os.path.join(REPO, "firmware", "src", "rom", "TSPICO-23.ROM")]
SETTINGS = {"rom": "TSPICO-23.ROM", "exrom": "tc2068-1.rom", "volume": 10,
            "sound": True, "tspico": True}
CARD_README = """This is the microSD card for ts2068-p4 %s
(https://github.com/timex-sinclair-projects/ts2068-p4).

Copy everything here to the root of a FAT32 card. TAP/, help/ and readme.tap
are the TS-Pico's own card files. TS2068/ is the emulator's:

  TS2068/settings.json   the ROM it starts with, volume, sound, TS-Pico on
  TS2068/ROMS/           tc2068-0.rom and tc2068-1.rom, the TS2068's HOME ROM
                         and EXROM (Timex's software is in the public domain;
                         these copies come with Fuse); TSPICO-23.ROM, the
                         TS-Pico ROM (MIT, from tspico-firmware-build)
  TS2068/SNAPS/          snapshots saved from the menu
  TS2068/DOCK/           DOCK cartridges for the menu's Cartridge: .dck files,
                         or a cartridge's ROM as it is (8K to 64K)
"""


def filesystem(st):
    fs = littlefs.LittleFS(**LFS)
    for root, dirs, files in os.walk(st):
        rel = os.path.relpath(root, st)
        dest = "/" if rel == "." else "/" + rel.replace(os.sep, "/")
        if dest != "/":
            fs.makedirs(dest, exist_ok=True)
        for name in sorted(files):
            with open(os.path.join(root, name), "rb") as f, \
                    fs.open(dest.rstrip("/") + "/" + name, "wb") as g:
                g.write(f.read())
    return bytes(fs.context.buffer)


def card(path, version):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        src = os.path.join(REPO, "firmware", "SD card")
        for root, dirs, files in os.walk(src):
            dirs.sort()
            for name in sorted(files):
                full = os.path.join(root, name)
                z.write(full, os.path.relpath(full, src))
        for rom in ROMS:
            z.write(rom, "TS2068/ROMS/" + os.path.basename(rom))
        z.writestr("TS2068/settings.json", json.dumps(SETTINGS) + "\n")
        z.writestr("TS2068/SNAPS/", "")
        z.writestr("TS2068/DOCK/", "")
        z.writestr("README.txt", CARD_README % version)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=BUILD)
    ap.add_argument("--out", default=os.path.join(REPO, "dist"))
    ap.add_argument("--version")
    a = ap.parse_args()
    version = a.version or subprocess.run(["git", "-C", REPO, "describe", "--tags", "--dirty"],
                                          capture_output=True, text=True, check=True).stdout.strip()
    os.makedirs(a.out, exist_ok=True)

    st, _ = install.stage(None)
    vfs = os.path.join(a.out, "vfs.bin")
    with open(vfs, "wb") as f:
        f.write(filesystem(st))
    shutil.rmtree(st)

    name = "ts2068-p4-%s" % version
    image = os.path.join(a.out, name + ".bin")
    subprocess.run(["esptool", "--chip", "esp32p4", "merge-bin", "-o", image,
                    "--flash-mode", "dio", "--flash-freq", "40m", "--flash-size", "16MB",
                    "--pad-to-size", "16MB",
                    "0x2000", os.path.join(a.build, "bootloader", "bootloader.bin"),
                    "0x8000", os.path.join(a.build, "partition_table", "partition-table.bin"),
                    "0x10000", os.path.join(a.build, "micropython.bin"),
                    "0x%x" % VFS_OFFSET, vfs], check=True)
    os.remove(vfs)
    with zipfile.ZipFile(os.path.join(a.out, name + ".zip"), "w", zipfile.ZIP_DEFLATED) as z:
        z.write(image, os.path.basename(image))
    card(os.path.join(a.out, "ts2068-p4-sd-%s.zip" % version), version)
    for n in sorted(os.listdir(a.out)):
        print("%10d  %s" % (os.path.getsize(os.path.join(a.out, n)), os.path.join(a.out, n)))


if __name__ == "__main__":
    main()
