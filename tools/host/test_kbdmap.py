#!/usr/bin/env python3
"""Check usermods/ts2068/kbdmap.py, the HID keyboard to TS2068 matrix map.

    python3 tools/host/test_kbdmap.py
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "usermods", "ts2068"))
import kbdmap                                                    # noqa: E402

SHIFT, CTRL, ALT = 0x02, 0x01, 0x04


def rep(mod=0, *codes):
    return bytes([mod, 0] + list(codes) + [0] * (6 - len(codes)))


CASES = [
    ("nothing", rep(), set()),
    ("a", rep(0, 0x04), {"A"}),
    ("A (Shift)", rep(SHIFT, 0x04), {"A", "CAPS"}),
    ("right Shift", rep(0x20, 0x1D), {"Z", "CAPS"}),
    ("1", rep(0, 0x1E), {"1"}),
    ("0", rep(0, 0x27), {"0"}),
    ("@ (Shift+2)", rep(SHIFT, 0x1F), {"SYM", "2"}),
    ("( (Shift+9)", rep(SHIFT, 0x26), {"SYM", "8"}),
    (") (Shift+0)", rep(SHIFT, 0x27), {"SYM", "9"}),
    ("* (Shift+8)", rep(SHIFT, 0x25), {"SYM", "B"}),
    ("^ (Shift+6)", rep(SHIFT, 0x23), {"SYM", "H"}),
    ("Enter", rep(0, 0x28), {"ENTER"}),
    ("space", rep(0, 0x2C), {"SPACE"}),
    ("quote", rep(SHIFT, 0x34), {"SYM", "P"}),
    ("apostrophe", rep(0, 0x34), {"SYM", "7"}),
    ("comma", rep(0, 0x36), {"SYM", "N"}),
    ("full stop", rep(0, 0x37), {"SYM", "M"}),
    ("colon", rep(SHIFT, 0x33), {"SYM", "Z"}),
    ("semicolon", rep(0, 0x33), {"SYM", "O"}),
    ("plus", rep(SHIFT, 0x2E), {"SYM", "K"}),
    ("equals", rep(0, 0x2E), {"SYM", "L"}),
    ("minus", rep(0, 0x2D), {"SYM", "J"}),
    ("slash", rep(0, 0x38), {"SYM", "V"}),
    ("question mark", rep(SHIFT, 0x38), {"SYM", "C"}),
    ("less than", rep(SHIFT, 0x36), {"SYM", "R"}),
    ("backquote: nothing", rep(0, 0x35), set()),
    ("Backspace", rep(0, 0x2A), {"CAPS", "0"}),
    ("Shift+Backspace too", rep(SHIFT, 0x2A), {"CAPS", "0"}),
    ("left", rep(0, 0x50), {"CAPS", "5"}),
    ("down", rep(0, 0x51), {"CAPS", "6"}),
    ("up", rep(0, 0x52), {"CAPS", "7"}),
    ("right", rep(0, 0x4F), {"CAPS", "8"}),
    ("Esc: BREAK", rep(0, 0x29), {"CAPS", "SPACE"}),
    ("Tab: extended mode", rep(0, 0x2B), {"CAPS", "SYM"}),
    ("Caps Lock", rep(0, 0x39), {"CAPS", "2"}),
    ("F1: EDIT", rep(0, 0x3A), {"CAPS", "1"}),
    ("Ctrl+P: SYMBOL SHIFT+P", rep(CTRL, 0x13), {"SYM", "P"}),
    ("Alt+3", rep(ALT, 0x20), {"SYM", "3"}),
    ("Ctrl+Shift+G", rep(CTRL | SHIFT, 0x0A), {"SYM", "CAPS", "G"}),
    ("Ctrl alone", rep(CTRL), {"SYM"}),
    ("Ctrl+Shift alone", rep(CTRL | SHIFT), {"SYM", "CAPS"}),
    ("Shift alone: nothing", rep(SHIFT), set()),
    ("keypad 5", rep(0, 0x5D), {"5"}),
    ("keypad 0", rep(0, 0x62), {"0"}),
    ("Shift+keypad 5 stays 5", rep(SHIFT, 0x5D), {"5", "CAPS"}),
    ("keypad Enter", rep(0, 0x58), {"ENTER"}),
    ("keypad +", rep(0, 0x57), {"SYM", "K"}),
    ("two keys", rep(0, 0x04, 0x05), {"A", "B"}),
    ("rollover codes ignored", rep(0, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01), set()),
]


def main():
    bad = 0
    for name, report, want in CASES:
        got = kbdmap.keys(report)
        if got != want:
            bad += 1
            print("FAIL %s: got %s, want %s" % (name, sorted(got), sorted(want)))
    # rows(): A is half-row 1 bit 0, CAPS half-row 0 bit 0
    r = kbdmap.rows(rep(SHIFT, 0x04))
    if r != bytes([0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]):
        bad += 1
        print("FAIL rows(Shift+A): %s" % r.hex())
    if kbdmap.rows(rep()) != b"\xff" * 8:
        bad += 1
        print("FAIL rows(nothing)")
    if not kbdmap.rollover(rep(0, 1, 1, 1, 1, 1, 1)) or kbdmap.rollover(rep(0, 4)):
        bad += 1
        print("FAIL rollover")
    print("test_kbdmap: %s (%d cases)" % ("PASS" if not bad else "FAIL (%d)" % bad,
                                         len(CASES) + 3))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
