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

## D5. Z80 core: Fuse's (2026-10-30)

**Decided:** the emulator runs on **Fuse's Z80 core** (`z80/`). Z80_JLS (`z80_jls/`) stays in the tree behind the same interface, so the comparison can be rerun, but nothing links it by default.

**How they were compared:** both cores sit behind `z80/z80_core.h` and reach the machine only through `z80/z80_glue.h`. The machine, `coretest`, `zexrun` and the P4 benchmark are the same code on either; `make CORE=jls` (host) and `TS2068_CORE=jls` (P4 build) pick Z80_JLS. Z80_JLS is ESPectrum v1.4.15's `Z80_JLS.cpp` with its ESPectrum hooks taken out (z80_jls/README.md); its instructions are unchanged.

| | Fuse | Z80_JLS |
|---|---|---|
| P4 frame, BASIC loop (core 0, 2,500 frames) | **2,511 µs** (15% of 16,635) | 2,928 µs (18%) |
| P4 boot frames | 2,334 µs | 2,865 µs |
| Host frame (Apple silicon, `-O2`) | **42 µs** | 49 µs |
| zexdoc, host | **32 s** | 40 s |
| zexdoc / zexall | 67/67, 67/67 | 67/67, 67/67 |
| `coretest` against Fuse's expected results | 1,356/1,356 | 1,345/1,356 |
| Machine against fuse-tspico, 3 ROMs, 1–3,000 frames | match | match |
| RAM after the P4 benchmark | same as host | same as host |
| App image | 0x1ccfe0 | 0x1bff70 (53 KB smaller) |
| License | GPL-2.0-or-later | GPL-3.0 |

Both P4 runs used the same board, the same day, and the same machine code, with emulated RAM in internal memory. Both cores were built at IDF's `-O2`; Fuse's runs from flash, and Z80_JLS's step loop and its block instructions keep ESPectrum's `IRAM_ATTR`.

**Why:** Fuse's core is faster, by about 14% on the P4 and 15% on the host. It is also more accurate. The 11 `coretest` results where Z80_JLS differs:

- 5 are not a fault. JR cc and DJNZ, when not taken, read their displacement in Z80_JLS and only contend for it in Fuse. The timing is the same. (The real Z80 does read it.)
- 5 are the block-instruction findings published in 2018: when INIR, OTIR, INDR, OTDR or CPDR repeat, flags 3 and 5 come from PC, P and H change, and MEMPTR becomes PC+1. Fuse emulates this; Z80_JLS doesn't. Only an interrupted block instruction shows it.
- 1 is a Z80_JLS bug: `OUT (n),A` with n = FFh sets MEMPTR to (A+1)×256 instead of A×256 (`REG_WZ |= (work8 + 1)` carries into the high byte).

None of these changed a single register, T-state or screen in the TS2068 runs, so the machine matched Fuse on both cores. Fuse also keeps the project GPL-2.0-or-later, and it's the code base the machine was lifted from. Gate 2's tie rule would have chosen it anyway.

**Gate 2 (met 2026-10-30):** Fuse's core holds a sustained frame of 2.5 ms against a 16.6 ms budget, which leaves about 85% for display and audio, and passes zexdoc, zexall and Fuse's `coretest`.

**Ruled out:** Z80_JLS, for the reasons above. Making the project GPL-3 bought nothing.

## D6. Display path: triple buffers in PSRAM, copied by the PPA (2026-10-30)

**Decided:** `platform/p4_display.c` drives the EK79007 panel directly through IDF's `esp_lcd` MIPI-DSI driver:

- three 1024×600 RGB565 frame buffers in PSRAM, flipped at refresh;
- the TS2068's picture at 640×480, every line doubled, with a 384-pixel side panel;
- only changed lines converted, in internal RAM, then copied into the frame buffer by the PPA, twice (even rows and odd rows);
- the panel's refresh tuned to 60.10 Hz to match the TS2068.

The numbers are in NOTES.md, "Phase 3 progress". This answers Q9: the worst frame is 9.9 ms of the 16.6 ms budget, with the Z80 included.

**Why:**

- **Triple buffering:** two buffers would mean either waiting for the panel or dropping frames. Since the panel and the TS2068 run at almost the same rate, the drops would come in long runs.
- **The PPA:** the CPU can only fill PSRAM at about 56 MB/s, which made a full redraw 11 ms.
- **The even/odd copies:** they double lines without relying on the PPA's scaler, whose method isn't documented.

**Ruled out:**

- **LVGL** (Elecrow's examples use it): a GUI library between the emulator and a frame buffer it fills itself.
- **The `esp_lcd_ek79007` managed component:** seven register writes don't justify a component-manager dependency in MicroPython's build.
- **Drawing through `esp_lcd_panel_draw_bitmap` copies:** they always go into the buffer currently on screen.

**Still open:** following the beam (Q7). The picture is drawn once a frame from screen RAM.

