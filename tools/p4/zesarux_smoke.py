# Run on the board (mpremote run tools/p4/zesarux_smoke.py, with
# tools/p4/session.py at /session.py): the zesarux-smoke session
# (test/traces), the firmware's tools/emu/smoke.py step for step, on the
# live machine. Keeps every TS-Pico bus frame and writes them to
# /capture.bin; prints its checks as smoke.py does. Compare on the host:
#
#   mpremote cp :/capture.bin capture.bin
#   python3 tools/host/compare_frames.py --timing capture.bin test/traces/zesarux-smoke.frames
#
# The flash and card must hold the trace's seed (test/traces/seeds/README.md).

import os
import time
import emu
import ts2068
import session as S

results = []


def check(cond, msg):
    results.append(bool(cond))
    print(("  PASS  " if cond else "  FAIL  ") + msg)


def sd_file(path):
    with emu._Card():
        try:
            with open("/sd" + path, "rb") as f:
                return f.read()
        except OSError:
            return None


ts2068.pico_trace(400000)
emu.start(rom="TSPICO-23.ROM")
time.sleep(3)

print("CAT")
t = S.run(S.CAT)
check("000 hello.tap" in t and "<GAMES>" in t and "0 OK" in t, "lists the card")

print('LOAD "tpi:hello.tap", LOAD "", LIST')
S.run(S.LOAD, '"tpi:hello.tap"')
t = S.run(S.LOAD, '""')
check("Program: hello" in t and "0 OK" in t, "mounted and loaded")
t = S.run(S.LIST)
check("HELLO FROM THE TS-PI" in t, "the program arrived intact")

print('SAVE "saved"')
S.run(S.SAVE, '"saved"', until="press any key", timeout=15)
S.key(S.ENTER)
t0 = time.ticks_ms()
while time.ticks_diff(time.ticks_ms(), t0) < 20000:
    time.sleep_ms(500)
    if "0 OK" in S.text():
        break
s = sd_file("/TAP/saved.tap")
check(s is not None and len(s) > 40, "saved.tap is on the card")

print('SAVE "tpi:info"')
t = S.run(S.SAVE, '"tpi:info"')
check("interface status" in t and "0 OK" in t, "status screen")

print('SAVE "tpi:picopt", LPRINT')
S.run(S.SAVE, '"tpi:picopt"')
S.run(S.LPRINT, '"LPRINT through the emulator"')
S.run(S.SAVE, '"tpi:path"')
s = sd_file("/VLPRINT/PRN0001.TXT")
check(s is not None and b"LPRINT through the emulator" in s, "printer text on the card")

print('SAVE "tpi:cd" (the menu, then n)')
S.run(S.SAVE, '"tpi:cd"', until="quit?", timeout=30)
S.key(S.N)
time.sleep(3)
check("0 OK" in S.text(), "n cancels the menu")
t = S.run(S.SAVE, '"tpi:path"')
check("/TAP" in t and "0 OK" in t, "and the next command reads a clean link")

print('SAVE "tpi:cd" (the menu, then 0)')
t = S.run(S.SAVE, '"tpi:cd"', until="quit?", timeout=30)
check("0 GAMES" in t, "the folder menu")
S.key(S.ZERO)
time.sleep(3)
t = S.text()
check("Changing dir to:" in t and "0 OK" in t, "a key picks the folder")
t = S.run(S.SAVE, '"tpi:path"')
check("/TAP/GAMES" in t, "and the Pico is in it")

time.sleep(2)
data = ts2068.pico_trace()
ts2068.pico_trace(0)
with open("/capture.bin", "wb") as f:
    f.write(data)
print("frames", len(data) // 3)
print("%s (%d checks)" % ("ALL PASS" if all(results) else "FAILURES", len(results)))
