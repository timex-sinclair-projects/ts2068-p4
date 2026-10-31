# osd: the side panel's menu, above the on-screen keyboard (PLAN.md
# Phase 3, "OSD"), on a thread of its own.
#
#   import osd
#   osd.start()                  # starts the machine (emu.start) if need be
#
# The rows and taps are ts2068.osd() and ts2068.osd_tap() (platform/
# p4_osd.c); the files and settings are emu.py's. The main page:
#
#   TS2068  <the ROM>
#   Load ROM          a ROM from /TS2068/ROMS, kept as the setting
#   Load snapshot     from /TS2068/SNAPS
#   Save snapshot     to SNAPS, as SNAPnnn.szx, the next number free
#   Reset             the machine started again from the settings
#   Keyboard          pair a Bluetooth keyboard, or forget it (btkbd.py)
#   Volume  -  10  +
#
# A list page shows seven names at a time, with Back, up and down. The
# bottom row is the status line; the keyboard's progress shows there as it
# changes.

import os
import time
import _thread

import btkbd
import emu
import ts2068

ROWS = 10
COLS = 31
PLAIN, INVERSE, BUTTON = 0, 1, 2
LIST_ROWS = 7                           # rows 1-7 of a list page

_page = None                            # "main", "roms", "snaps" or "kbd"
_items = []
_top = 0
_status = ""
_running = False


def _row(r, text, style=PLAIN):
    ts2068.osd(r, text[:COLS], style)


def _say(text):
    global _status
    _status = text
    _row(ROWS - 1, " " + text)


def _main():
    global _page
    _page = "main"
    s = emu.settings()
    _row(0, " TS2068  " + s["rom"], INVERSE)
    _row(1, "")
    _row(2, "  Load ROM", BUTTON)
    _row(3, "  Load snapshot", BUTTON)
    _row(4, "  Save snapshot", BUTTON)
    _row(5, "  Reset", BUTTON)
    _row(6, "  Keyboard", BUTTON)
    _row(7, "  Volume    -   %3d   +" % s["volume"])
    _row(8, "")
    _say(_status)


def _list(page, title, items):
    global _page, _items, _top
    _page, _items, _top = page, items, 0
    _row(0, " " + title, INVERSE)
    _show_list()


def _show_list():
    for i in range(LIST_ROWS):
        n = _top + i
        if n < len(_items):
            _row(1 + i, "  " + _items[n], BUTTON)
        else:
            _row(1 + i, "  (none)" if n == 0 else "")
    _row(8, "  < Back      up       down")
    _say("%d-%d of %d" % (_top + 1 if _items else 0,
                          min(_top + LIST_ROWS, len(_items)), len(_items)))


def _roms():
    """What can be booted: 32K files, and 16K HOME ROMs named *-0.* whose
    *-1.* EXROM is there too. -> [(name, exrom or None)]"""
    names = emu.roms()
    out = []
    with emu._Card():
        for n in names:
            size = os.stat(emu.DIR + "/ROMS/" + n)[6]
            if size == 0x8000:
                out.append((n, None))
            elif size == 0x4000 and "-0." in n and n.replace("-0.", "-1.") in names:
                out.append((n, n.replace("-0.", "-1.")))
    return out


def _next_snap_name():
    have = set(n.upper() for n in emu.snapshots())
    for i in range(1, 1000):
        n = "SNAP%03d.SZX" % i
        if n not in have:
            return n
    raise OSError("SNAPS is full")


def _tap_main(row, col):
    global _roms_found
    if row == 2:
        _roms_found = _roms()
        _list("roms", "Load ROM", [n for n, _ in _roms_found])
    elif row == 3:
        _list("snaps", "Load snapshot", emu.snapshots())
    elif row == 4:
        name = _next_snap_name()
        _say("Saving...")
        emu.save_snapshot(name)
        _say("Saved " + name)
    elif row == 5:
        _say("Resetting...")
        emu.start()
        _main()
        _say("Reset")
    elif row == 6:
        name = btkbd.saved()
        _list("kbd", "Keyboard", ["Pair a keyboard"] + (["Forget " + name] if name else []))
    elif row == 7:
        s = emu.settings()
        v = s["volume"]
        if 10 <= col <= 14:
            v = max(0, v - 5)
        elif 20 <= col <= 24:
            v = min(100, v + 5)
        else:
            return
        ts2068.volume(v)
        emu.save(volume=v)
        _row(7, "  Volume    -   %3d   +" % v)


def _tap_list(row, col):
    global _top
    if row == 8:
        if col < 10:
            _main()
        elif col < 19:
            _top = max(0, _top - LIST_ROWS)
            _show_list()
        elif _top + LIST_ROWS < len(_items):
            _top += LIST_ROWS
            _show_list()
        return
    n = _top + row - 1
    if not 1 <= row <= LIST_ROWS or n >= len(_items):
        return
    if _page == "kbd":
        _main()
        if n == 0:
            btkbd.pair()
        else:
            btkbd.forget()
        _say(btkbd.status())
    elif _page == "roms":
        name, exrom = _roms_found[n]
        _say("Starting " + name)
        changes = {"rom": name}
        if exrom:
            changes["exrom"] = exrom
        emu.save(**changes)
        emu.start()
        _main()
        _say("Started " + name)
    else:
        _say("Loading " + _items[n])
        emu.load_snapshot(_items[n])
        _main()
        _say("Loaded " + _items[n])


def _loop():
    kbd = btkbd.status()
    while True:
        if btkbd.status() != kbd:
            kbd = btkbd.status()
            _say(kbd)
        tap = ts2068.osd_tap()
        if tap:
            try:
                (_tap_main if _page == "main" else _tap_list)(*tap)
            except Exception as e:
                _say("Error: %s" % e)
        else:
            time.sleep_ms(30)


def start():
    """Start the machine if it isn't running, and the menu once"""
    global _running
    if not ts2068.run_status()["running"]:
        emu.start()
    _main()
    if not _running:
        _running = True
        _thread.stack_size(16 * 1024)
        _thread.start_new_thread(_loop, ())
