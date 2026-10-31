# emu: start the TS2068 (the ts2068 module) from the microSD card's
# TS2068 folder (PLAN.md Phase 3, "Storage").
#
#   import emu
#   emu.start()                      # as settings.json says
#   emu.start(rom="TSPICO-23.ROM")   # this once, without saving it
#   emu.save(volume=20)              # change settings.json
#   emu.settings()                   # -> dict
#   emu.roms()                       # -> the files in ROMS/
#
# On the card:
#
#   /TS2068/settings.json   rom, exrom, volume, sound
#   /TS2068/ROMS/           ROM files
#
# rom names a file in ROMS/: a 16K HOME ROM, with exrom naming its 8K or
# 16K EXROM, or a 32K file holding both, HOME first (as TSPICO-23.ROM
# does), when exrom is ignored. The card is the TS-Pico's too: it's
# mounted only while the files are read, so the TS-Pico firmware can mount
# it itself. tools/p4/install.py puts this file on the flash, and
# tools/p4/sd_roms.py puts ROMs on the card.

import json
import machine
import os

import ts2068

SD = "/sd"
DIR = SD + "/TS2068"
DEFAULTS = {"rom": "tc2068-0.rom", "exrom": "tc2068-1.rom", "volume": 10, "sound": True}


class _Card:
    """Mounted at /sd for the with block, unless it already was."""

    def __enter__(self):
        self.card = None
        try:
            os.stat(SD)
            return self
        except OSError:
            pass
        with open("/p4/sd.json") as f:
            a = json.load(f)
        a["data"] = tuple(a["data"])
        self.card = machine.SDCard(**a)
        try:
            os.mount(self.card, SD)
        except OSError:
            self.card.deinit()
            raise
        return self

    def __exit__(self, *exc):
        if self.card:
            os.umount(SD)
            self.card.deinit()


def _read_settings():
    s = dict(DEFAULTS)
    try:
        with open(DIR + "/settings.json") as f:
            s.update(json.load(f))
    except OSError:
        pass
    return s


def settings():
    with _Card():
        return _read_settings()


def save(**changes):
    with _Card():
        s = _read_settings()
        s.update(changes)
        try:
            os.mkdir(DIR)
        except OSError:
            pass
        with open(DIR + "/settings.json", "w") as f:
            json.dump(s, f)
    return s


def roms():
    with _Card():
        try:
            return sorted(os.listdir(DIR + "/ROMS"))
        except OSError:
            return []


def _load(name):
    with open(DIR + "/ROMS/" + name, "rb") as f:
        return f.read()


def start(**overrides):
    """Read the settings and ROMs, then run the machine (ts2068.run)."""
    with _Card():
        s = _read_settings()
        s.update(overrides)
        home = _load(s["rom"])
        if len(home) == 0x8000:
            home, ex = home[:0x4000], home[0x4000:]
        else:
            ex = _load(s["exrom"])
    if len(home) != 0x4000:
        raise ValueError("%s: not 16K or 32K" % s["rom"])
    ts2068.volume(s["volume"])
    ts2068.run(home, ex, False, False, s["sound"])
    return s
