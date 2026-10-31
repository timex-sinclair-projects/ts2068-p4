# A third board: the ESP32-P4 (proposal draft)

Status: **opened upstream 2026-10-31 as https://github.com/timex-sinclair-projects/tspico-firmware-build/issues/354.** This is the firmware change that PLAN.md §1.3 calls for. Once it's agreed it becomes an issue, then a PR, in `tspico-firmware-build`. It's based on what the Phase 1 prototype needed: `tspico/py/p4host.py` and its stand-ins, with the firmware pinned at `58ccbcd` (v2.3.1). Checked again on 2026-10-31 against upstream main, `f37fc8c` (v2.3.1-61): every line number below is the same on both, and the only change that matters here is four new board names (change 5).

## Why

The P4 emulator runs the unmodified TS-Pico firmware in MicroPython on core 1. The emulated Z80 reaches ports 0Eh/0Fh through `p4bridge`, a C module whose `p4bridge.MQ` behaves like PIO0 SM0's `TS_IO_DUAL`, as `tsbus.MQ` does on the v3 card (docs/DECISIONS.md D4 here). Phase 1 proved this works: both Phase 0 bridge traces replay with matching replies (docs/NOTES.md, "Gate 1").

But today the prototype runs the firmware under stand-ins. `p4host.py` replaces `rp2`, `machine`, `TS.sdcard` and `select` in `sys.modules` before the firmware imports, as `tools/emu/pico_host.py` does. Any firmware change can break a stand-in without anyone noticing. Since phase 4 of the v3 port, the firmware has a board layer (`TS/board.py`; docs/v3-board-layer-proposal.md). Almost everything the stand-ins do is a board function that already exists, so the P4 fits as a third board.

## What the stand-ins had to cover

| Stand-in | Why it was needed | Upstream home |
|---|---|---|
| `rp2` (`StateMachine`, `asm_pio`, `PIO`) | `tspico_io.py` 8 imports `rp2` at module level and decorates `TS_IO_DUAL`, `set_ctrl`, `sel_bank` with `@asm_pio` at import. The P4 has no `rp2`, which makes it the first board that isn't an RP2. | Guard the import (change 2) |
| `StateMachine(0, TS_IO_DUAL)` → `p4bridge.MQ` | Making MQ | `board.make_mq()`: already a board function |
| `machine` without `mem32` | **Hazard:** `tspico_io.py` 109 takes `machine.mem32` if it exists, and `MQX` then writes "PIO0 SM0's INSTR register" at 0x502000D8. On the P4, `mem32` exists and that address is arbitrary memory. Today only a `tsbus` import turns this off (113-127). | "MQ is not PIO" from the board (change 1) |
| `machine.Pin`, `SPI` no-ops | `tspico_io.py` 13 imports them; v2's SD and LED code calls them | `board_p4` doesn't need them (change 2 makes the import harmless) |
| `TS.sdcard.SDCard(spi, cs)` → the board's SDMMC card | `ACTIVATE_SD` (tspico.py 1119-1120) builds the SPI driver itself. The P4's microSD slot is SDMMC (slot 0, 1-bit, CLK 43, CMD 44, D0 39), not SPI. | A new `board.sd_open()` (change 3) |
| `select.poll` that never reports input | `DRAIN_STDIN` (`tspico_io.py` 352) reads USB stdin to catch Ctrl-C. Under `p4host` the REPL shares that console, so the firmware ate REPL input. | A board flag (change 4) |
| `fw.machine`, `dev_tspico` | `dev_extcmd` and `machine.reset` lookups | Nothing needed once the stand-ins go |
| COMPAT values: `statvfs`, `uname`, `gc.mem_free`, `sys.implementation`, `BUILD_VERSION` | So the replies match pico_host's traces byte for byte (`tpi:info`, `tpi:dir` sizes) | **Not upstream.** These are for testing only, and stay in this repo's replay harness. |

Nothing else was needed. The protocol code never touched hardware outside `MQ`, `board.*` and the list above. The firmware's own pacing works unchanged against a queue that reports at most 4. Both the trace replay and v3's `tsbus` port showed this.

## The changes

**1. One place says which board, importable by everyone.** `board.py` chooses between v2 and v3 by trying to import `tsbus`. `tspico_io.py` does the same test separately, because `board_v2` imports `tspico_io`, so `tspico_io` can't import the board. Add `TS/board_id.py`, which imports nothing from `TS`:

```python
# Which board, decided once; TS/board.py and TS/tspico_io.py both read it.
try:
    from tsbus import MQ as _m
    NAME, PIO_MQ = "v3", False
except ImportError:
    try:
        from p4bridge import MQ as _m
        NAME, PIO_MQ = "p4", False
    except ImportError:
        NAME, PIO_MQ = "v2", True
```

`board.py` then imports `board_v2`, `board_v3` or `board_p4` according to `NAME`. `tspico_io.py`'s guard at 113-127 becomes `if not board_id.PIO_MQ: _DMA = None; _mem32 = None`, and `DRAIN_MAX` keys off the same flag. No behaviour changes on v2 or v3.

**2. `tspico_io.py` imports without `rp2`.** Wrap line 8 (and the `machine` imports on 13) in `try/except ImportError`, with a decorator that does nothing in place of `asm_pio`, so the three PIO programs are defined but never assembled. v2 is unchanged because the import succeeds, and so is v3 (the RP2350 has `rp2`). Another option is moving the PIO programs into `board_v2.py`. That's cleaner, but a bigger diff, and it touches the upgrade UF2's module list (src/CLAUDE.md, "tspico_io.py is also built into the upgrade UF2"). Start with the guard.

**3. `board.sd_open()`: the board makes the block device.** `ACTIVATE_SD` currently calls `board.sd_spi()`, `board.sd_cs()` and then `SDCard(spi, cs)`. Change it to `sd = board.sd_open()`. The default in v2 and v3 is exactly today's code (`SDCard(sd_spi(), sd_cs())`), so they don't change. `board_p4.sd_open()` returns the P4's card (`machine.SDCard(slot=0, width=1, ...)`) wrapped with a `CID` attribute. `sd_cs`/`sd_spi` remain board functions for v2/v3, and for `tools/pico-serial.py --sd`.

The CID is an open question for the P4. MicroPython's esp32 `SDCard` doesn't expose the CID register. The firmware already treats `CID = 0` as "unknown" (tspico.py 1198-1210, since the 2026-09-30 audit), so `board_p4` can start with 0. Card-swap detection then relies on the card being seen missing. Exposing the CID (a small `sdmmc_card_t` accessor in the port, or in `p4bridge`) can follow.

**4. `board.DRAIN_STDIN`** (True on v2/v3, False on P4). It gates `tspico_io.DRAIN_STDIN`. On the P4 the console belongs to the emulator, not the firmware.

**5. `TS/board_p4.py`**, the same names as `board_v2`:

| Name | P4 |
|---|---|
| `NAME`, `PIO_MQ`, `SLOTS`, `HAS_CORE1` | `"p4"`, False, False (the emulator owns memory and paging), False (MicroPython's threads share core 1 under the GIL; core 0 is the emulator's) |
| `LABEL`, `SD_SLOTS`, `HAS_MODEM`, `HAS_SPEECH` (new since v2.3.1) | `"P4"`, False, False, False. With the two flags False, `tspico.py` never calls the v3-only `modem_*`, `speech_start` or `say`, so `board_p4` doesn't define them. The P4 has audio, so speech could come later through the emulator's sound path. |
| `early_init()` | nothing (the CPU clock is IDF's) |
| `make_mq()` | `p4bridge.MQ()`, with both queues emptied |
| `restart_mq(MQ)` | nothing |
| `start_memory`, `map_slots`, `rom_slot_empty` | nothing, nothing, False (the DOCK/EXROM slots are the emulator's; Phase 5 decides whether `tpi:boot`/`tpi:dock` map onto it) |
| `card_present()`, `sd_card_ready()` | None, True (no detect switch on the CrowPanel) |
| `sd_take_bus()`, `sd_release_bus()` | `make_mq()` without emptying (the card has its own pins); nothing |
| `sd_open()` | see change 3 |
| `make_led()`, `led_brightness()` | an object whose `on`/`off`/`value`/`toggle` do nothing (or an on-screen indicator later); nothing |
| `background(fn, args)` | `_thread.start_new_thread` |

**6. `EMULATOR_BRIDGE.md`: the in-process transport.** Document the contract that `tsbus.MQ` and `p4bridge.MQ` both meet and the firmware relies on:

- 9-bit RX words, with bit 8 marking a write to 0Fh.
- Y drops to BUSY *before* each Z80 write is queued.
- `rx_fifo()`/`tx_fifo()` report at most 4.
- `exec()` understands the five `MQX` instructions.
- An empty TX reads 00h after a bounded wait.

This replaces PLAN.md's idea of a shared Python `BusModel`. On both non-PIO boards the model is in C, and a written contract plus a conformance test serve better than shared code. The test could be a host test that drives any MQ implementation through the cases `pico_host`'s selftest already covers. Propose it as bridge v1.1: the frame ops don't change.

## What this repo does afterwards

- `p4host.py` shrinks to: load `/p4/sd.json`, set `TLM_ENABLED`, start `TS2068_IO` on a thread. The `rp2`, `machine`, `TS.sdcard` and `select` stand-ins are deleted.
- The COMPAT values move into `tspico/py/p4replay.py`, so only the trace replay uses them.
- PLAN.md Phase 3: "Switch from the temporary stand-ins to the upstream platform layer" becomes doable.

## Questions for the firmware maintainers

1. Should `board_id.py` exist, or would you rather `tspico_io` asked `board` lazily (at first `MQX`) and broke the import cycle that way?
2. Should change 2 be the import guard, or move the PIO programs into `board_v2.py` now?
3. Should P4 support live in the firmware repo (`board_p4.py` tested by a host test with a fake `p4bridge`), or should the firmware only offer the hooks, with `board_p4.py` here? The first keeps it from rotting. The second keeps P4 code out of the UF2 build.
4. Should a host test cover the MQ contract (change 6)?

## Testing plan

- v2 and v3: the existing host tests and `reference_hosttest.py` (each change updates `docs/reference/`, per the firmware's CLAUDE.md), plus the usual hardware check on a v2.2 board and the v3 card.
- P4: both Phase 0 traces replay with no mismatches, with the stand-ins removed and the COMPAT values only in the replay harness.
