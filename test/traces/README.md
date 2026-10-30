# Reference bridge traces

Recorded 2026-10-30 for PLAN.md Phase 0. These are the fixtures that Phase 1's C test driver replays through `bridge_frame()`, and that Phase 4 compares the P4 against.

Both runs use the real TS-Pico firmware under `pico_host.py` with `TSPICO_TRACE=1`. Both passed every check, finished with `underruns: 0`, and had no firmware exception.

| Fixture | Emulator | Script | Covers | Frames |
|---|---|---|---|---|
| `fuse-smoke.*` | fuse-tspico `tspico-v1.10.0-1`, headless (`--with-null-ui --with-audio-driver=null --enable-automation`) | firmware `tools/emu/fuse_smoke.py` | boot, HELLO, `LOAD ""` of a test program served as `/assets/nofile.tap`, which runs `CAT`, mounts `tpi:fusedata.tap`, `LOAD "" CODE 40000`, `VERIFY` | 2,486 |
| `zesarux-smoke.*` | zesarux-tspico lab build (`~/Documents/github/zesarux-tspico-lab/work/zesarux-tspico`, built 2026-10-04) | firmware `tools/emu/smoke.py` | `CAT`; mount and `LOAD` a BASIC TAP; **`SAVE "saved"`**; `SAVE "tpi:info"`; the virtual printer (`tpi:picopt`, `LPRINT`); the `tpi:cd` menu, cancelled and used | 2,192 |

Firmware: tspico-firmware-build `v2.3.1` (`58ccbcd`), from the `firmware` submodule, with `tools/build-basic.sh` run first to generate `src/assets/*.tap` (git-ignored there).

**Why two emulators.** The plan names fuse-tspico + `pico_host` as the bridge reference. Fuse has no remote control, and its debugger can't answer SAVE's "press any key", so `fuse_smoke.py` leaves SAVE out. The SAVE session therefore comes from ZEsarUX, the other bridge-v1 client. Bridge traces don't depend on which emulator sent the frames, but if a Fuse-recorded SAVE is wanted, the Fuse debugger script needs a way to press a key through the keyboard port.

## Files

- `*.log`: the full `pico_host` output: firmware telemetry (`[TLM …]`), host messages and every bus frame.
- `*.frames`: only the bus frames, one per line: `OP VALUE -> REPLY`, all in hex. `OP` is `HELLO`, `OUT0E`, `IN0E`, `IN0F` or `OUT0F` (bridge ops 4, 0, 1, 2, 3).
- `fuse-smoke.result.json`, `fuse-smoke.screen.png`: Fuse automation's result and final screen.

## Caveats for replay

- **Status reads are thinned.** `pico_host` logs an `IN0F` only when the status byte changes (`TRACE` comment in `pico_host.py`). A Z80 polling loop reads 0Fh many more times than the trace shows. A replay driver must therefore poll `IN0F` until it returns the next recorded value (with a timeout), rather than send exactly one read per line.
- **No timing.** The bridge doesn't model bus timing (EMULATOR_BRIDGE.md). Replies from `IN0E` can depend on whether the firmware has queued a byte yet; `pico_host` waits up to 50 ms (`TX_WAIT`) before an empty FIFO reads 00h. The P4 bridge should match that behaviour, or a replay may see 00h where the trace has data.
- **The Fuse screenshot is drawn twice.** `fuse-smoke.screen.png` shows the 2068 screen twice side by side, each copy cut off at the right. The run itself is correct: the checks read memory and ERR_NR, not the picture. It looks like Fuse's automation screen capture mishandles the TS2068's double-width display buffer. That's relevant to Q7 (how Fuse renders TS2068 hi-res); not investigated yet.

## Reproducing

```bash
# fuse-tspico and libspectrum built from the pinned submodules into ~/esp/ref (see docs/NOTES.md)
cd firmware && ./tools/build-basic.sh
FUSE=~/esp/ref/fuse-tspico/fuse TSPICO_TRACE=1 python3 tools/emu/fuse_smoke.py --keep   # -> /tmp/fuse_pico_host.out
TSPICO_TRACE=1 python3 tools/emu/smoke.py --keep                                         # -> /tmp/pico_host.out
python3 -c 'import re,sys; print("".join("%s %s -> %s\n" % f for f in re.findall(r"\[bus\] (\w+) ([0-9A-F]{2}) -> ([0-9A-F]{2})", open(sys.argv[1]).read())), end="")' LOG > NAME.frames
# Not a line-based grep: pico_host prints frames and telemetry from two threads, so a
# telemetry line can be glued onto the end of a frame line.
```
