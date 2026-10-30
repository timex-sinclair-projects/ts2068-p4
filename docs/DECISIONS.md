# Decisions

What was decided, why, and what was ruled out (PLAN.md §5). Newest last.

---

## D1. Own project, borrowing modules (2026-10-25)

**Decided:** a new P4 platform layer and project structure, with the TS2068 machine and a Z80 core lifted as C modules, and the real TS-Pico firmware on MicroPython beside it.

**Ruled out:** porting Fuse, ZEsarUX or ESPectrum whole; TSRun as a base (no license); a Wi-Fi bridge to `pico_host` (development aid only); rewriting the firmware in C. Reasons in PLAN.md §1.4.

## D2. Reference sources are pinned submodules (2026-10-30)

**Decided:** `firmware` and everything in `third_party/` are git submodules pinned to a tag or commit, listed in `third_party/PROVENANCE.md`. Code we lift is copied into our own tree, so the submodules stay read-only.

**Why:** reproducible lifts, and a clear record of which upstream commit each lifted file matches when sending fixes back.

**Pins:** firmware `v2.3.1`, fuse-tspico `tspico-v1.10.0-1`, libspectrum `2d95bbd`, ZX-ESPectrum-IDF `v1.4.15`.

## D3. MicroPython and ESP-IDF versions (2026-10-30, provisional)

**Proposed:** MicroPython **v1.29.0** (latest release, 2026-08-24) with ESP-IDF **v5.5.2**, the version its `ports/esp32/README.md` recommends.

**Why not v1.27:** PLAN.md names v1.27 because that's when P4 support arrived, not as a requirement. v1.29.0 splits the P4 board into revision 3+ and `PRE_REV3` variants, which matters for this board (see below).

**Source:** `ports/esp32/README.md` at each tag:

| MicroPython | Recommended ESP-IDF | Also supported |
|---|---|---|
| v1.27.0 | v5.5.1 | v5.2, v5.2.2, v5.3, v5.4, v5.4.1, v5.4.2 |
| v1.28.0 | v5.5.1 | v5.3, v5.4, v5.4.1, v5.4.2 |
| v1.29.0 | v5.5.2 | v5.3, v5.4, v5.4.1, v5.4.2, v5.5.1, v5.5.4 |

**Board variant:** `ESP32_GENERIC_P4`, variant **`PRE_REV3_C6_WIFI`**. Our board's chip is revision v1.3, read with esptool on 2026-10-30 (NOTES.md, "Chip revision").

**Settled 2026-10-30:** the stock build runs a REPL on the board (NOTES.md, "Stock MicroPython on the board").

## D4. Phase 1 integration: approach A, with the bus model in C (2026-10-30)

**Decided:** approach **A** from PLAN.md Phase 1: build inside MicroPython's esp32 port (v1.29.0), with our code as a USER_C_MODULES native module, `p4bridge`. MicroPython's task and all its `_thread` threads run on **core 1** (`ports/esp32/mphalport.h:51-55`, `MP_TASK_COREID` is 1 on dual-core chips and boards can't override it; `mpthreadport.c:157` pins threads to the same core). `p4bridge` starts plain C tasks pinned to **core 0**, which never take the GIL or touch MicroPython objects.

**The bus model is in C, not Python.** `p4bridge` holds the model of PIO0 SM0 (`TS_IO_DUAL`) that `pico_host.py`'s `BusModel` implements: 9-bit RX words, the status byte Y, BUSY after every Z80 write, a TX FIFO that reports at most 4, `pull(noblock)`, and an `IN (0Eh)` that waits up to 50 ms for a byte before reading 00h. The emulator side calls `p4bridge_frame(op, value)` in C, on core 0, against FreeRTOS queues. The firmware side is a Python object with `rp2.StateMachine`'s methods (`get`, `put`, `rx_fifo`, `tx_fifo`, `exec`, `active`, `restart`), which the `rp2` stand-in returns for `StateMachine(0, TS_IO_DUAL)`.

**Why:** PLAN.md says to port `BusModel` to MicroPython, with a Python thread answering frames from a queue. That would put a Python thread switch on core 1 in the path of every Z80 `IN`/`OUT` to 0Eh/0Fh, and the firmware's main loop would compete for the GIL with it. In C, only an `IN (0Eh)` with nothing queued waits for Python, which is also how the real hardware and the v3 card behave: the RP2040's PIO and the v3 card's `tsbus` C module (`firmware/src/TS/board_v3.py`) answer the Z80 without Python. The semantics stay exactly `BusModel`'s, so the Phase 0 traces still apply.

**One deliberate ordering change:** on a Z80 write, Y is cleared to BUSY *before* the word is queued (pico_host clears it after). Otherwise the firmware, running on the other core, can read the word and set READY before the BUSY lands, and the READY is lost. The Z80 can't see the difference: the frame is atomic from its side.

**What stays Python, temporarily:** the stand-ins for `rp2`, `machine` (`Pin`, `SPI`, `freq`; **no `mem32`**), `TS.sdcard` (a wrapper around `machine.SDCard`), `select` and `os`/`sys`/`gc` reporting, installed in `sys.modules` before the firmware is imported, as `pico_host.py` does. They live in `tspico/py/`. PLAN.md §1.3's upstream platform layer replaces them later. The firmware already has a board layer (`TS/board.py` choosing `board_v2` or `board_v3`), so the upstream change is likely a `board_p4.py` plus a small `TS/board.py` selection, not a new layer.

**Hazard found:** `firmware/src/TS/tspico_io.py` writes RP2040 PIO registers (`0x502000D8`, …) through `machine.mem32` whenever `mem32` exists and `tsbus` doesn't. MicroPython on the P4 has `machine.mem32`, so the firmware must **never** be imported on the P4 without the `machine` stand-in in place. Upstream, a P4 board needs to switch that path off explicitly.

**Ruled out for now:** approach B (embedding MicroPython in our own ESP-IDF app). A gave us core pinning and our own C tasks without trouble, so there's no reason to switch yet.
