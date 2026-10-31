# emu: start the TS2068 (the ts2068 module) from the microSD card's
# TS2068 folder (PLAN.md Phase 3, "Storage").
#
#   import emu
#   emu.start()                      # as settings.json says; a reset if running
#   emu.start(rom="TSPICO-23.ROM")   # this once, without saving it
#   emu.save(volume=20)              # change settings.json
#   emu.settings()                   # -> dict
#   emu.roms()                       # -> the files in ROMS/
#   emu.save_snapshot("game")        # the running machine -> SNAPS/game.szx
#   emu.load_snapshot("game")        # starts the machine first if need be
#   emu.snapshots()                  # -> the files in SNAPS/
#   emu.update_firmware()            # TS/ on the card -> /TS, if it's new
#
# On the card:
#
#   /TS2068/settings.json   rom, exrom, volume, sound, tspico
#   /TS2068/ROMS/           ROM files
#   /TS2068/SNAPS/          SZX snapshots (ts2068.snapshot); Fuse's load
#                           too, if they need no DOCK cartridge
#   /TS2068/TS/             a TS-Pico firmware update: a copy of src/TS
#                           (.py or .mpy), installed the next time the
#                           firmware starts
#
# rom names a file in ROMS/: a 16K HOME ROM, with exrom naming its 8K or
# 16K EXROM, or a 32K file holding both, HOME first (as TSPICO-23.ROM
# does), when exrom is ignored. A 32K ROM plugs in the TS-Pico too, unless
# tspico is false: the firmware (p4host.py, /TS) starts the first time,
# and ports 0Eh/0Fh go to it. The card is the TS-Pico's too: it's
# mounted only while the files are read, so the TS-Pico firmware can mount
# it itself. tools/p4/install.py puts this file on the flash, and
# tools/p4/sd_roms.py puts ROMs on the card.

import json
import machine
import os
import sys
import time

import ts2068

SD = "/sd"
DIR = SD + "/TS2068"
SNAPS = DIR + "/SNAPS"
DEFAULTS = {"rom": "tc2068-0.rom", "exrom": "tc2068-1.rom", "volume": 10, "sound": True,
            "tspico": True}
READY_MS = 15000                        # the firmware's boot, to READY


class _Card:
    """Mounted at /sd for the with block, unless it already was."""

    def __enter__(self):
        self.card = self.mounted = None
        try:
            os.stat(SD)
            return self
        except OSError:
            pass
        # The TS-Pico firmware's card, once it has one: the slot takes one
        # SDCard at a time (p4host keeps its own for good)
        host = sys.modules.get("p4host")
        if host and host._card:
            self.mounted = host._card
        else:
            with open("/p4/sd.json") as f:
                a = json.load(f)
            a["data"] = tuple(a["data"])
            self.card = self.mounted = machine.SDCard(**a)
        try:
            os.mount(self.mounted, SD)
        except OSError:
            if self.card:
                self.card.deinit()
            raise
        return self

    def __exit__(self, *exc):
        if self.mounted:
            os.umount(SD)
        if self.card:
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


def _szx(name):
    return name if name.lower().endswith(".szx") else name + ".szx"


def snapshots():
    with _Card():
        try:
            return sorted(os.listdir(SNAPS))
        except OSError:
            return []


def save_snapshot(name):
    """The machine as it is now, between frames, to SNAPS/name.szx"""
    data = ts2068.snapshot()
    with _Card():
        for d in (DIR, SNAPS):
            try:
                os.mkdir(d)
            except OSError:
                pass
        with open(SNAPS + "/" + _szx(name), "wb") as f:
            f.write(data)
    return len(data)


def load_snapshot(name, **overrides):
    """Restore SNAPS/name.szx, starting the machine as start() does if it
    isn't running. A snapshot holds no ROM: it runs on the settings' ROMs."""
    with _Card():
        with open(SNAPS + "/" + _szx(name), "rb") as f:
            data = f.read()
    if not ts2068.run_status()["running"]:
        start(**overrides)
    ts2068.load_snapshot(data)


def _load(name):
    with open(DIR + "/ROMS/" + name, "rb") as f:
        return f.read()


def start(**overrides):
    """Read the settings and ROMs, then run the machine (ts2068.run),
    stopping it first if it's running: a reset."""
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
    # A 32K ROM is the TS-Pico's: plug the TS-Pico in, the firmware first
    pico = s["tspico"] and len(ex) == 0x4000 and _tspico()
    ts2068.stop()
    ts2068.volume(s["volume"])
    ts2068.run(home, ex, False, False, s["sound"], pico)
    return s


def _rmtree(path):
    try:
        names = os.listdir(path)
    except OSError:
        return
    for n in names:
        p = path + "/" + n
        if os.stat(p)[0] & 0x4000:
            _rmtree(p)
        else:
            os.remove(p)
    os.rmdir(path)


def update_firmware():
    """A firmware update from the card: if /TS2068/TS holds .py or .mpy
    files that aren't what was last installed from there, they replace the
    flash's /TS (the old one kept as /TS.old). True if it updated. The card
    copy is checked by a hash of its names and contents, kept in
    /TS/.card. .py files load more slowly than .mpy, which must come from
    an mpy-cross of the same MicroPython."""
    import hashlib
    import binascii
    src = DIR + "/TS"
    with _Card():
        try:
            names = sorted(n for n in os.listdir(src)
                           if n.endswith(".py") or n.endswith(".mpy"))
        except OSError:
            return False
        if not names:
            return False
        h = hashlib.sha256()
        for n in names:
            h.update(n + "\0")
            with open(src + "/" + n, "rb") as f:
                h.update(f.read())
        digest = binascii.hexlify(h.digest()).decode()
        try:
            with open("/TS/.card") as f:
                if f.read().strip() == digest:
                    return False
        except OSError:
            pass
        print("emu: installing the TS-Pico firmware from the card (%d files)" % len(names))
        _rmtree("/TS.new")
        os.mkdir("/TS.new")
        for n in names:
            with open(src + "/" + n, "rb") as f, open("/TS.new/" + n, "wb") as g:
                g.write(f.read())
    with open("/TS.new/.card", "w") as f:
        f.write(digest + "\n")
    _rmtree("/TS.old")
    try:
        os.rename("/TS", "/TS.old")
    except OSError:
        pass
    os.rename("/TS.new", "/TS")
    return True


def _tspico():
    """Start the TS-Pico firmware (tspico/py/p4host.py) if it isn't running,
    and wait for it to be READY. Once started it runs until a hard reset,
    across machine resets, as the Pico does on a TS2068 reset. A firmware
    update on the card goes in first (update_firmware)."""
    import p4host
    if getattr(p4host, "_started", False):
        return True                     # READY was seen; the Z80 may have taken it
    update_firmware()
    p4host.start()
    p4host._started = True
    t = time.ticks_ms()
    while not p4host.ready():
        if time.ticks_diff(time.ticks_ms(), t) > READY_MS:
            raise OSError("TS-Pico firmware not READY")
        time.sleep_ms(50)
    return True
