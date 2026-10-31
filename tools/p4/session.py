# session: type BASIC into the running TS2068 and read its screen, on the
# board: the P4's counterpart of the firmware's tools/emu/session.py, which
# does the same to ZEsarUX over ZRCP, and in the same way. Each line is
# typed as placeholder keys, then written into the edit line already
# tokenised, since the 2068's keyword entry is too fiddly to type.
#
#   import session as S
#   S.run(S.CAT)                         # -> the screen as text, once a report shows
#   S.run(S.SAVE, '"tpi:info"')
#   S.key(S.ENTER)
#
# Needs ts2068.run() going (emu.start). tools/p4/zesarux_smoke.py uses it.

import time
import ts2068

# BASIC tokens (as session.py)
SAVE, LOAD, CAT, CODE, LPRINT, PRINT, LIST = 0xF8, 0xEF, 0xCF, 0xAF, 0xE0, 0xF5, 0xF0
E_LINE, ERR_NR = 23641, 23610

# Keys: (half-row, bit) in the matrix as the machine reads it
ENTER, A, N, ZERO = (6, 0), (1, 0), (7, 3), (4, 0)


def key(k, hold=100):
    """Press and let go of one key"""
    rows = bytearray(b"\xff" * 8)
    rows[k[0]] &= ~(1 << k[1])
    ts2068.keys(rows)
    time.sleep_ms(hold)
    ts2068.keys()
    time.sleep_ms(hold)


def text():
    return "\n".join(l.rstrip() for l in ts2068.screen_text().split("\n"))


def peek16(addr):
    b = ts2068.peek(addr, 2)
    return b[0] | b[1] << 8


def wait_editor(timeout=30):
    """Past a report: the K cursor alone on the bottom line, pressing
    ENTER until it's there"""
    t0 = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), t0) < timeout * 1000:
        lines = [l for l in text().split("\n") if l.strip()]
        if lines and lines[-1].strip() == "K":
            return
        key(ENTER)
        time.sleep_ms(600)
    raise AssertionError("the editor never came up:\n" + text())


def run(*parts, until=None, timeout=60):
    """Enter one immediate line given as tokens (ints) and text (str).
    Waits until a report is printed (or `until` shows), then returns the
    screen text."""
    line = b"".join(bytes([p]) if isinstance(p, int) else p.encode() for p in parts)
    wait_editor()
    # The editor drops a key that comes while it's still busy with the
    # last one, so type, count what arrived, and top up
    t0 = time.ticks_ms()
    while True:
        el = peek16(E_LINE)
        cur = ts2068.peek(el, len(line) + 1)
        have = cur.find(b"\x0d")
        if have == len(line):
            break
        assert 0 <= have < len(line), "typing failed: " + cur.hex()
        assert time.ticks_diff(time.ticks_ms(), t0) < 30000, "typing failed: " + cur.hex()
        for _ in range(len(line) - have):
            key(A)
        time.sleep_ms(300)
    ts2068.poke(el, line)
    ts2068.poke(ERR_NR, b"\xff")
    key(ENTER)
    t0 = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), t0) < timeout * 1000:
        time.sleep_ms(500)
        t = text()
        if until is not None:
            if until in t:
                return t
        elif ts2068.peek(ERR_NR)[0] != 0xFF or ", 0:1" in t:
            return t
    return text()
