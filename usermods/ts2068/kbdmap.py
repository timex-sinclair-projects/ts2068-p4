# kbdmap: a USB/Bluetooth HID keyboard's boot report as the TS2068's key
# matrix (PLAN.md Phase 3, "Input, stage 2"). Plain Python, so the host
# can test it (tools/host/test_kbdmap.py).
#
#   rows = kbdmap.rows(report)    # 8 bytes: modifiers, 0, six key codes
#                                 # -> 8 half-rows, 0 bits down, as
#                                 #    ts2068.keys() takes them
#
# A PC keyboard types what its keys say:
#
#   letters, digits, space, Enter   the same keys; Shift is CAPS SHIFT
#   punctuation, Shift+digit        the TS2068 symbol it prints, through
#                                   SYMBOL SHIFT (Shift+2 is SYMBOL SHIFT+2, @)
#   Ctrl or Alt                     SYMBOL SHIFT itself, for the keywords
#                                   and symbols on the keys (Ctrl+Shift:
#                                   extended mode's two shifts)
#   Backspace, Delete               DELETE (CAPS SHIFT+0)
#   arrows                          CAPS SHIFT+5 to 8
#   Esc                             BREAK (CAPS SHIFT+SPACE)
#   Tab                             extended mode (CAPS SHIFT+SYMBOL SHIFT)
#   Caps Lock                       CAPS LOCK (CAPS SHIFT+2)
#   F1 F2 F3 F4                     EDIT, GRAPHICS, TRUE VIDEO, INV. VIDEO
#   keypad                          its digits and + - * / . Enter

# Where each key is: (half-row, bit). Half-row n is the one port FE reads
# with address line A(8+n) low.
_KEY = {}
for _row, _names in enumerate((
        ("CAPS", "Z", "X", "C", "V"), ("A", "S", "D", "F", "G"),
        ("Q", "W", "E", "R", "T"), ("1", "2", "3", "4", "5"),
        ("0", "9", "8", "7", "6"), ("P", "O", "I", "U", "Y"),
        ("ENTER", "L", "K", "J", "H"), ("SPACE", "SYM", "M", "N", "B"))):
    for _bit, _name in enumerate(_names):
        _KEY[_name] = (_row, _bit)

_SHIFT, _CTRL_ALT = 0x22, 0x55          # modifier bits: L/R Shift; Ctrl, Alt

# Key codes (HID usage page 7) that are a key of the TS2068's
_PLAIN = {0x28: "ENTER", 0x2C: "SPACE", 0x58: "ENTER"}
for _i in range(26):
    _PLAIN[0x04 + _i] = chr(65 + _i)
for _i in range(9):
    _PLAIN[0x1E + _i] = str(_i + 1)     # top row
    _PLAIN[0x59 + _i] = str(_i + 1)     # keypad
_PLAIN[0x27] = _PLAIN[0x62] = "0"

# Shift+digit on a PC: the symbol, through SYMBOL SHIFT
_SHIFTED_DIGIT = {"1": "1", "2": "2", "3": "3", "4": "4", "5": "5",
                  "6": "H", "7": "6", "8": "B", "9": "8", "0": "9"}

# Punctuation: (unshifted, shifted), each the key SYMBOL SHIFT goes with,
# or None
_SYMBOL = {
    0x2D: ("J", "0"),       # - _
    0x2E: ("L", "K"),       # = +
    0x2F: ("Y", "F"),       # [ {
    0x30: ("U", "G"),       # ] }
    0x31: ("D", "S"),       # \ |
    0x33: ("O", "Z"),       # ; :
    0x34: ("7", "P"),       # ' "
    0x35: (None, "A"),      # ` ~
    0x36: ("N", "R"),       # , <
    0x37: ("M", "T"),       # . >
    0x38: ("V", "C"),       # / ?
    0x54: ("V", "V"),       # keypad /
    0x55: ("B", "B"),       # keypad *
    0x56: ("J", "J"),       # keypad -
    0x57: ("K", "K"),       # keypad +
    0x63: ("M", "M"),       # keypad .
}

# Keys that are a CAPS SHIFT combination whatever the modifiers
_CAPS = {0x2A: "0", 0x4C: "0", 0x50: "5", 0x51: "6", 0x52: "7", 0x4F: "8",
         0x29: "SPACE", 0x2B: "SYM", 0x39: "2",
         0x3A: "1", 0x3B: "9", 0x3C: "3", 0x3D: "4"}


def keys(report):
    """The TS2068 keys a boot report holds down, as a set of names"""
    mod = report[0]
    shift, raw = mod & _SHIFT, mod & _CTRL_ALT
    down = set()
    for code in report[2:8]:
        if code < 4:                    # none, or rollover/error codes
            continue
        if code in _CAPS:
            down.update(("CAPS", _CAPS[code]))
        elif code in _SYMBOL:
            k = _SYMBOL[code][1 if shift else 0]
            if k:
                down.update(("SYM", k))
        elif code in _PLAIN:
            k = _PLAIN[code]
            if raw:
                down.update(("SYM", k))
                if shift:
                    down.add("CAPS")
            elif shift and k in _SHIFTED_DIGIT and code < 0x59:
                down.update(("SYM", _SHIFTED_DIGIT[k]))
            else:
                down.add(k)
                if shift:
                    down.add("CAPS")
    if raw and not down:
        down.add("SYM")                 # Ctrl or Alt alone: SYMBOL SHIFT
        if shift:
            down.add("CAPS")
    return down


def rows(report):
    """The half-rows for a boot report: 8 bytes, 0 bits down"""
    out = bytearray(b"\xff" * 8)
    for name in keys(report):
        r, b = _KEY[name]
        out[r] &= ~(1 << b)
    return bytes(out)


def rollover(report):
    """True for a report saying too many keys are down (keep the last)"""
    return report[2] == 0x01
