# Notes

Working notes: findings, measurements and things to check. Settled facts move to PLAN.md §2 with their source.

---

## Chip revision: v1.3, pre-revision 3 (verified 2026-10-30)

MicroPython's `ESP32_GENERIC_P4/board.md` (v1.29.0) says the P4 changed substantially at chip revision 3.0, and that Espressif marks revision 3+ parts with an **X** suffix (`ESP32-P4NRW32X`).

`esptool flash-id` on our board, over the UART port:

```
Chip type:          ESP32-P4 (revision v1.3)
Features:           Dual Core + LP Core, 400MHz
Crystal frequency:  40MHz
Flash:              manufacturer ba, device 4018, 16MB
```

So this board needs the **`PRE_REV3_C6_WIFI`** variant. Our own sdkconfig must carry the same settings as MicroPython's `boards/sdkconfig.p4_pre_rev3`:

```
CONFIG_ESP32P4_REV_MIN_300=n
CONFIG_ESP32P4_REV_MIN_0=y
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
```

Other CrowPanel Advanced 7" units may ship with revision 3 chips. Firmware built this way runs on revision 0.x/1.x only; a rev 3 board needs the plain `C6_WIFI` variant and its own build.

## USB ports on the board (2026-10-30)

- **One USB-C port** goes to a WCH CH340 USB-serial chip (USB ID `1A86:7522`). This is the flashing and console port, wired to the P4's UART with auto-reset (esptool resets it into download mode via RTS/DTR). macOS 26 has no built-in driver for this CH340 model: install WCH's `CH34xVCPDriver` and approve its driver extension. The port then shows as `/dev/cu.wchusbserial10`.
- **The other USB-C port** goes to the P4's high-speed USB OTG port (480 Mbps link). The factory demo enumerates there as `303A:4004` "Advance-P4" by "Elecrow", with a single HID interface ("Elecrow HID interface") and no serial interface. That port is the one that could act as a USB host for a keyboard (Q3); whether it supplies VBUS to a device is still to be checked on the schematic.

## Local checkouts that already exist (2026-10-30)

In `~/Documents/github`:

- `ESP2068`: an earlier port of ESPectrum to the TS2068 (factus10/ESP2068, last commit 2026-07-11, "Phase 4 Piece D: confirm the ROM patches against a real, live Z80 core"). Not mentioned in PLAN.md. It may hold useful TS2068 ROM patch work and Z80_JLS findings, which are relevant to Phase 2.
- `fuse-tspico-lab`: a fuse-tspico working tree on `tspico-device` at `a3d27c57` (2026-10-06), two days newer than the `tspico-v1.10.0-1` tag pinned here. It already has a `build-null` build and split `tspico-home.rom` / `tspico-exrom.rom`. That's a head start on Phase 0's reference build.
- `zesarux-tspico-lab`: zesarux-tspico working tree.
- `tspico-firmware-build`: on branch `megabank-bus-sharing` at `fd9db5c`, ahead of the `v2.3.1` pinned here.

## Toolchain on this Mac (2026-10-30)

Present: git, gh, cmake, python3 (pyenv), perl, autoconf, automake, pkg-config.

**ESP-IDF v5.5.2** installed 2026-10-30:

- Source: `~/esp/esp-idf` (shallow clone of tag `v5.5.2`, 23 submodules), 649 MB.
- Tools: `~/.espressif` (`./install.sh esp32p4`), 2.6 GB. Compiler `riscv32-esp-elf-gcc` 14.2.0 (crosstool-NG esp-14.2.0_20251107); Python env `idf5.5_py3.14_env` on Python 3.14.0.
- Activate per shell with `. ~/esp/esp-idf/export.sh`. Its output must not be piped (`| tail` runs it in a subshell and loses the environment).
- Checked by building `examples/get-started/hello_world` for `esp32p4` with the three pre-rev3 settings and `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`: builds cleanly, and the resulting sdkconfig has `CONFIG_ESP32P4_REV_MIN_FULL=0`. Not flashed.

Standalone **esptool v5.6.0** is also installed (`uv tool install esptool`, at `~/.local/bin/esptool`). ESP-IDF's own env carries esptool 4.12.0, so v5 syntax (`flash-id`, `read-flash`) works only with the standalone one.

## Factory flash backup (2026-10-30)

The full 16 MB flash, as shipped by Elecrow, is in `backups/crowpanel-p4-factory-16MB.bin` (git-ignored, so it lives only on this Mac; copy it somewhere safe).

- SHA-256 `a6cca95985227934bb3c206c40bdbf67f5a2e46e99f02aa1eba7b7d968740b35`
- Checked with `esptool verify-flash 0 …` against the chip: digest matched.
- Factory partition table (at 0x8000): `nvs` 0x9000 24K, `phy_init` 0xf000 4K, `factory` app 0x10000 11M, `storage` spiffs 0xb10000 4M.

**Restore:**

```bash
esptool --port /dev/cu.wchusbserial10 --baud 230400 write-flash 0 backups/crowpanel-p4-factory-16MB.bin
```

**Serial speed limit:** the CH340 link fails at 460800 and 921600 baud ("Corrupt data, expected 0x1000 bytes"), on every attempt. 230400 is reliable. Use `--baud 230400` (or `-b 230400` with `idf.py`) for all flashing; a full 16 MB read takes about 13 minutes. Reads were done in 1 MB chunks with retries, so one glitch costs one chunk.

## Stock MicroPython on the board (2026-10-30)

MicroPython **v1.29.0**, board `ESP32_GENERIC_P4`, variant `PRE_REV3_C6_WIFI`, built in `~/esp/micropython/ports/esp32` with ESP-IDF v5.5.2. App image 1,772,016 bytes (259,600 bytes left in its partition). Flashed after `erase-flash`, at 230400 baud:

```bash
esptool --chip esp32p4 --port /dev/cu.wchusbserial10 --baud 230400 write-flash --flash-mode dio --flash-size 16MB --flash-freq 40m \
  0x2000 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin 0x10000 micropython.bin
```

Boot log: `ESP-ROM:esp32p4-eco2-20240710`, then the banner `MicroPython v1.29.0 on 2026-10-30; Generic ESP32P4 module with pre revision 3 chip and ESP32C6 hosted WIFI with ESP32-P4`. The REPL answers on the CH340 port at 115200.

What the REPL reports:

| | |
|---|---|
| `machine.freq()` | **360,000,000** (see below) |
| `sys.implementation._thread` | `'GIL'`: `_thread` works, threads share one GIL |
| GC heap | 64,000 bytes at start, growing into PSRAM: `gc.mem_free()` 33,090,208 |
| Partitions | `nvs` 24K at 0x9000, `phy_init` 4K at 0xf000, `vfs` 14 MB at 0x200000 |
| Filesystem `/` | 3,584 × 4K blocks, 3,581 free |

**CPU clock: 360 MHz, not 400.** ESP-IDF v5.5.2's `components/esp_system/port/soc/esp32p4/Kconfig.cpu` makes 360 MHz the default when `ESP32P4_SELECTS_REV_LESS_V3` is set, and offers the 360 MHz option only for those chips. 400 MHz is still selectable in Kconfig, but whether revision 1.3 silicon is rated for it needs Espressif's datasheet or errata, not a guess. Until that's settled, **Phase 2's speed budget assumes 360 MHz**, 10% less than PLAN.md assumed.

**For Phase 1:** `_thread` is GIL-based, so the firmware's threads and the REPL all share core 1 time. Which core MicroPython's task and its threads are pinned to is still to be read from `ports/esp32` (`MP_TASK_COREID`, and how `mp_thread_create` pins).

## Reference build: fuse-tspico + pico_host (2026-10-30)

Built from the pinned submodules, outside the repo, in `~/esp/ref`:

- `~/esp/ref/libspectrum`: clone of `third_party/libspectrum` at `2d95bbd`, `./configure --prefix=~/esp/ref/prefix`.
- `~/esp/ref/fuse-tspico`: clone of `third_party/fuse-tspico` at `tspico-v1.10.0-1`, `./configure --with-null-ui --with-audio-driver=null --enable-automation` with `PKG_CONFIG_PATH=~/esp/ref/prefix/lib/pkgconfig`. Reports Fuse 1.10.0.

Checks, all passing:

- fuse-tspico's own `tspico/bridge_test.py` (stand-in bridge, stock ROMs): 5/5.
- firmware `tools/emu/fuse_smoke.py` with the real firmware: 6/6 (boot, HELLO, CAT, mount, LOAD CODE, VERIFY).
- firmware `tools/emu/smoke.py` in ZEsarUX: 13/13 (adds SAVE, tpi:info, printer, tpi:cd menu).

Gotcha: at firmware `v2.3.1`, `src/assets/*.tap` are generated, not committed. Run `firmware/tools/build-basic.sh` (needs `zmakebas`) before either smoke test, or `fuse_smoke.py` fails on the missing `src/assets`. The outputs are git-ignored, so the submodule stays clean.

The ROM at `v2.3.1` is `src/rom/TSPICO-23.ROM`, not the `TSPICO-21.ROM` PLAN.md §2 names. Phase 3's ROM split should use whichever the pinned firmware carries.

Traces are in `test/traces/` (see its README).

## Phase 1 progress (2026-10-30)

**Board facts from Elecrow's V1.2 files** (repo `Elecrow-RD/CrowPanel-Advanced-7inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen`: schematic `Eagle_SCH&PCB/1.2/*.sch`, examples under `example/V1.2/idf-code/`). Our board's PCB revision isn't confirmed yet; check the silkscreen.

- **microSD:** SDMMC slot 0, **1-bit**: CLK GPIO43, CMD GPIO44, D0 GPIO39 (DAT1-3 not wired). No card detect. Card VDD on the 3.3 V rail; the pads (VDDPST_5) are fed by the P4's LDO4, MicroPython's default SD LDO channel on P4. Elecrow's own readme says the schematic puts SD on GPIO14-19; that's wrong, those go to the C6. In `tools/p4/board.json`.
- **USB-C:** J1 → CH340K → UART0 (GPIO37/38), with auto-reset. J16 → the P4's USB OTG pins. **J16's VBUS only feeds the board through diodes**, so a USB keyboard on J16 would need a powered hub or a VBUS-injecting adapter (Q3; inferred from the schematic).
- **C6 link:** SDIO slot 1, 4-bit, GPIO14-19. Elecrow's example sdkconfig sets the C6 reset to GPIO54, but the schematic's C6_EN net is on **GPIO32**.
- **Peripherals:** EK79007 MIPI-DSI panel, backlight PWM GPIO31; GT911 touch (I2C SDA 45, SCL 46, RST 40, INT 42; 0x5D or 0x14); LCD reset GPIO41; two NS4168 I2S amps (LRCLK 21, BCLK 22, DATA 23, enable GPIO30, active low); PDM mic (CLK 24, DATA 26).

**MicroPython with p4bridge** (D4): built with
`make BOARD=ESP32_GENERIC_P4 BOARD_VARIANT=PRE_REV3_C6_WIFI USER_C_MODULES=<repo>/usermods/micropython.cmake` in `~/esp/micropython/ports/esp32` (since 2026-10-31, `BOARD_DIR=<repo>/boards/CROWPANEL_P4_7` instead: Phase 3, "Our own board"). App 1,776,016 bytes (255,600 left). FreeRTOS tick is 100 Hz (`CONFIG_FREERTOS_HZ=100`), and the idle-task watchdog is off on both cores, so a spinning task on core 0 is allowed. A user module's qstrs must not clash with frozen ones: a dict key named `n` broke the build (`redeclaration of enumerator 'MP_QSTR_n'`).

Checked from the REPL: HELLO → 1; Y reads 00 after an `OUT`; RX words 0x55 and 0x103 (port 0Fh); `put(b"AB")` and `put(0x143)` → `IN 0Eh` reads 41, 42, 43; an empty `IN 0Eh` reads 00 after 49 ms and counts an underrun. 1,000 status frames from Python take 14 ms (14 µs each, most of it the Python call).

**The firmware on the P4:** `tools/p4/install.py` puts the pinned firmware (`TS/*.mpy` from mpy-cross), its files, and `tspico/py` on the flash. `p4host.start()` imports it under the stand-ins in **0.6 s** and it reaches its main loop (READY, boot pre-load queued) in **5.3 s**, 2 s of which is five failed SD attempts with no card in. Telemetry matches pico_host's (`LOAD_CONFIG enter: build=58ccbcd (HEAD)`, `ACTIVATE_MQ exit: SM active, Y=BUSY`).

**Trace extraction bug, fixed:** pico_host prints frames and telemetry from two threads, so a telemetry line can be glued onto a frame line. A line-based grep left junk on one line of each `.frames` file (no frames were lost). Now extracted with a regex over the whole log (test/traces/README.md).

## Gate 1: the traces replay on the P4 (2026-10-30)

Both Phase 0 traces replay with every reply matching, through `p4bridge` on core 0 against the unmodified firmware on core 1, with the board's microSD card (reseeded by `seed_sd.run()` before each run: the traces write to the card).

| Trace | Frames | Telemetry off | Telemetry on | IN 0Eh | Underruns |
|---|---|---|---|---|---|
| fuse-smoke | 2486 | 0.44–0.53 s | 0.90 s | mean 23 µs, max 1.2 ms | 0 |
| zesarux-smoke | 2192 | 1.17–1.19 s | 2.37 s | mean 19 µs, max 3.1 ms | 0 |

Each was run at least three times, telemetry on and off, with no mismatches, status timeouts or RX overruns. The real Z80 sessions took far longer; the replay runs as fast as the firmware answers.

**Pacing rules.** A trace has no timestamps, so `replay.c` has to know when the real Z80 would have waited. It needs two rules:

- **Status polls:** an `IN 0Fh` is retried until it reads what the trace has. For a non-zero value, the firmware must also have taken every Z80 write so far (10 s limit).
- **Before every Z80 write** (`OUT 0Eh`/`0Fh`), the firmware must be blocked waiting for one, in `MQ.get()` or an empty `MQ.rx_fifo()`, **and that wait must have begun after it took every earlier write** (3 s limit). The second half matters. The firmware's "waiting" flag stays set for a few µs after a word wakes it, until core 1 runs. Without the second half, the replay sent an LPRINT character's SYNC while the firmware was still inside `PRINT_IO` for the last one. `PRINT_IO`'s closing READY then landed after the SYNC's BUSY, and the next pre-header was captured as `103 42 05 …`. The firmware took that for a non-printer command, flushed the printer to the card mid-printout, and ended in RECOVERED (FB). `p4bridge` now counts pushes and takes, so a wait only counts if it began with nothing queued.

A real Z80 never needs either rule: BASIC takes milliseconds between LPRINT characters, and the ROM waits for READY. But an emulator that runs ahead of real time between bus accesses could hit the same stale READY. Phase 3 should keep the emulator in real time around port 0Eh/0Fh, or check this again.

## Phase 2 progress (2026-10-30)

**Fuse's Z80 core is lifted and passes Fuse's core tester on the host** (`make -C test/z80 check`: 1,356 tests, every register, T-state and bus access). What changed from Fuse is in `z80/README.md`. The core now reaches the rest of the emulator only through `z80/z80_glue.h`.

**What the TS2068 machine pulls in from Fuse (Q6, read 2026-10-30, not built yet).** Beyond the files PLAN.md §2 lists:

- **Memory and machine:** `memory_pages.c` (817 lines, 2K pages, the `contended` flag; it also uses glib and hooks for Opus, Spectranet, TTX2000S, uSpeech and the debugger) and `spectrum.c` (`RAM[65][0x4000]` is 1,040 KB, but the TS2068 uses pages 0, 2 and 5; the frame event). From `machine.c`, only the timings, `line_times` and the contention-table fill.
- **Ports and ULA:**
  - `ula.c`: port FE, the beeper, `ula_contention[80000]` plus a no-MREQ table, 160 KB together.
  - `periph.c`: port lookup through a GSList.
  - `machines_periph.c`: the AY on F5/F6, with joysticks on R14 through A8/A9.
  - `tc2048.c`: `tc2048_port_from_ula`.
- **Display:**
  - Files: `display.c`, `display_dirty.c` (444 lines), `display_render.c` (356), `display_border.c` (226), `rectangle.c`.
  - Calls into our code: `uidisplay_plot8` and `uidisplay_plot16` (512 hi-res) on a 640×480 canvas.
- **Sound:**
  - Files: `ay.c`, `sound/ay_engine.c` (335), `blipbuffer.c` (623), `output_mixer.c` (343) and its four filters (about 310).
  - `sound.c`'s init and low-level parts need replacing with our own. `psg.c` isn't needed (PSG recording only).
- **To replace, not lift:**
  - `periph.c`: a switch on the low byte.
  - `event.c`: a fixed array.
  - `module.c` and the startup manager: explicit reset calls.
  - `settings`: a small struct.
  - stubs for the UI, debugger, RZX, the tape and SLT traps, `dck.c` and the memory pool.
  - libspectrum's generated header: `compat/libspectrum.h`.
- **Machine wiring:**
  - Ports: `scld.h`'s bitfields depend on `WORDS_BIGENDIAN` (both targets are little-endian). Port FE is fully decoded (`0x00ff`/`0x00fe`); F4 and FF go to the SCLD.
  - EXROM: loaded with allowed sizes 8K and 16K; the TS-Pico uses 16K.
  - Timing: 3,528,000 Hz CPU, AY at CPU/2, 224 × 262 = 58,688 T-states a frame, interrupt length 32, top-left pixel at T-state 9169.
- **DOCK/EXROM contention (PLAN.md §3):** `scld_set_exrom_dock_contention` is at `peripherals/scld.c:391-401`. Callers: `machines/tc2068.c:190` (TS2068 and TC2068 reset), `scld.c:278` (snapshot), `dck.c:195` and `:301`, and `spec_se.c:161`. HOME RAM 5 is contended by `memory_ram_set_16k_contention(5,1)` in `ts2068.c:91`.

**Q7, from reading the code: Fuse follows the beam in 8×1-pixel chunks.**

- `display_update_critical` copies the region up to the beam position (from `tstates` and `line_times`) before a write lands at or after it.
- `scld_dec_write` forces that on a mode change.
- Border changes are logged by beam position.
- Each chunk is drawn with the RAM and the port FF value at copy time (`display_render.c:58-128`).

**Z80_JLS (ESPectrum) as the other candidate, read 2026-10-30.**

- **What fits:** memory goes through replaceable function pointers, with contention charged on each access, so a page-keyed TS2068 hook fits without core changes.
- **What would change:**
  - I/O calls `Ports::` directly in 32 places.
  - The clock only advances inside `VIDEO::Draw`, and HALT signals the frame loop with `CPU::stFrame = 0`.
  - The LOAD and SAVE ROM traps assume Spectrum addresses and would misfire on the TS2068 ROM.
- **The cost:** the core is all-static C++.
- **Effort:** about 1–2 days to extract.
- **Testing:** MEMPTR, Q and the undocumented flags are implemented, but the repo has no test results, so it needs `test/z80`'s suite.

**The TS2068 machine is lifted and boots on the host (2026-10-30).** `machine/` is Fuse's TS2068, SCLD, paging, ports and contention, cut down into one file behind `machine/machine.h` (details in `machine/README.md`). `make -C test/machine compare` runs three ROMs here and in the reference fuse-tspico (`~/esp/ref`):

- **The ROMs:** the stock TS2068 ROM, the TS-Pico ROM (`TSPICO-23.ROM`, split, with no TS-Pico attached), and `test/basic/dockhome.asm`.
- **What's compared:** every Z80 register, R and the T-state within the frame, after 1, 2, 10, 50, 100, 300, 1,000 and 3,000 frames.
- **Result:** all identical, and so is the part of the screen Fuse's PNG holds (below).
- **Speed:** a frame takes about 80 µs on the Mac, `-O2`. That's no guide to the P4.

**The DOCK-vs-HOME timing program (`test/basic/dockhome.asm`)** counts loop passes per frame with one access under test at 0x4000, and leaves the counts in registers. In loop passes per frame:

| Test | Fuse | Ours, TS2068 contention |
|---|---|---|
| Read 0x4000, HOME RAM5 | 1332 | 1332 |
| Read 0x4000, EXROM paged in | 1332 | 1396 |
| Read 0x4000, DOCK paged in | 1332 | 1396 |
| Read 0x8000 (uncontended reference) | 1396 | 1396 |
| Run code at 0x4000, HOME | 1139 | 1139 |
| Run code at 0x4000, EXROM | 1139 | 1396 |
| IN from port 40FBh, HOME at 0x4000 | 1164 | 1164 |
| IN from port 40FBh, EXROM at 0x4000 | 1164 | 1248 (uncontended) |

With `fuse_dock_contention` set, ours matches Fuse's column exactly. On a real TS2068, the same ROM pair answers Q10's DOCK question: if DOCK and EXROM read 1396, they aren't contended. It needs a way to read the registers there; drawing the counts on screen would do.

**Fuse's automation PNG is garbled on Timex machines.**

- **The defect:** `automation/artifacts.c` captures the null UI's 640 × 480 framebuffer correctly, but then writes `screen.png` through Fuse's screenshot code with the "Double size" scaler. In the 1280 × 960 result, the left 640 pixels of each row hold only the left half of a line, and the right 640 repeat it two rows down. `test/traces/fuse-smoke.screen.png` shows it too.
- **The workaround:** `compare_fuse.py` compares only the left border and the screen's left 16 columns.
- **The fix,** in fuse-tspico, the user's fork: write the PNG from `screen_rgb`, the buffer it already captured. Not raised yet.

**P4 benchmark, Fuse's core (2026-10-30).** `usermods/ts2068` builds `machine/` and `z80/` into the MicroPython image as a `ts2068` module. `ts2068.bench()` runs the machine in a task pinned to core 0 (priority 5) with no display, audio or TS-Pico. It boots the stock ROM and types the fixed BASIC loop on the emulated keyboard: `10 FOR I=1 TO 9999: NEXT I`, `RUN` (`test/machine/basic_loop.c`). The loop runs from about frame 290 to frame 2908. `tools/p4/bench_ts2068.py` times frames 300 to 2800, then runs the same frames on the host and compares a hash of the 48K of RAM.

| | Per frame |
|---|---|
| Mean, BASIC loop (2,500 frames) | 2,515 µs |
| Min / max | 2,499 / 2,557 µs |
| Mean, booting (frames 0–300) | 2,330 µs |
| Budget (one 60.1145 Hz frame) | 16,635 µs |

- **Headroom:** 15% of the budget used, about 6.6× real time. No frame came close to the budget, and two runs agreed to within 2 µs.
- **Correctness:** the RAM after 2,800 frames is the same on the P4 and the host (FNV-1a `22bb9d65`).
- **Where things live:** the emulated RAM, the ROM copies and the contention table landed in internal L2MEM (`0x4FF4xxxx`) as plain `.bss`, with no attributes needed. The Z80 core runs from flash through the cache: `z80_do_opcodes` is 106 KB, with the prefixed opcodes inlined (`HAVE_ENOUGH_MEMORY`). Moving it to internal RAM, or building without `HAVE_ENOUGH_MEMORY`, could be tried if the platform layer needs the time, but there's no need yet.
- **Gotcha:** MicroPython's esp32 port exports `machine_init`, which collided at link time. The machine API is now prefixed `ts2068_`.
- **Space:** the app partition is now 93% full: `micropython.bin` is 0x1ccf40 of 0x1f0000, leaving 140 KB (0x1ccfe0 after the core interface was added). Phase 3's platform layer will need a bigger partition table or a smaller image.

Gate 2 wasn't met yet at this point: zexdoc/zexall and the Z80_JLS comparison were still to do. (Both done below.)

**zexdoc and zexall pass on Fuse's core (2026-10-30).**

- **The programs:** Frank Cringle's exercisers, from agn453/ZEXALL at `319f7f9` (GPL-2.0, as extracted from YAZE-AG 2.51.3), are in `test/z80/zex/`.
- **The runner:** `test/z80/zexrun.c` runs a `.com` on the lifted core in a flat 64K, with no contention. CP/M's BDOS (functions 2 and 9) and its warm boot are trapped through OUT instructions at 0x0005 and 0x0000.
- **Result:** both pass 67 of 67, so zexall's undocumented flags are right too. Each runs 46.7 billion T-states, about 32 s on the Mac.
- **Check on the runner:** with one bit of the first expected CRC flipped in a scratch copy, it reports `ERROR` for that test.

With coretest, the 2.5 ms P4 frame and the Fuse comparisons, Fuse's core now meets every Gate 2 condition. What was left in Phase 2 was the comparison the plan asks for: Z80_JLS behind the same interface, then the decision in `docs/DECISIONS.md`.

**Z80_JLS behind the same interface (2026-10-30).**

- **The interface:** `z80/z80_core.h` declares `z80_init`, `z80_reset`, `z80_do_opcodes`, `z80_interrupt`, `z80_frame`, `z80_get_regs`/`z80_set_regs` and `z80_core_name`. Its conventions are Fuse's. `z80/z80_core_fuse.c` adds the last four for Fuse's core. `machine/ts2068.c`, `test/z80/coretest.c` and `test/z80/zexrun.c` now use only this, and no Fuse register macros.
- **Choosing a core:** `z80/core.mk` takes `CORE=fuse` (the default) or `CORE=jls` in both host test Makefiles. Z80_JLS is compiled as C++17 without exceptions or RTTI and linked with `cc`, so it needs no C++ runtime. Its binaries get a `-jls` suffix. `compare_fuse.py --core jls` runs those. On the P4, `TS2068_CORE=jls` in the environment picks it when cmake configures. The choice is fixed at configure time, so give each core its own build directory:

  ```
  TS2068_CORE=jls make BOARD=ESP32_GENERIC_P4 BOARD_VARIANT=PRE_REV3_C6_WIFI \
    BUILD=build-ESP32_GENERIC_P4-PRE_REV3_C6_WIFI-jls USER_C_MODULES=... -j8
  ```

- **The lift:** `z80_jls/README.md`. A script removed ESPectrum's hooks: TR-DOS paging, the tape traps, the video and CPU loop. The instructions are unchanged.
- **Gotcha:** a `status()` key named `core` failed the build with `redeclaration of enumerator 'MP_QSTR_core'` in `frozen_content.c`. The key is `z80`.

| | Fuse | Z80_JLS |
|---|---|---|
| P4 frame, BASIC loop | 2,511 µs (min 2,494, max 2,560) | 2,928 µs (min 2,912, max 2,986) |
| P4 boot frames | 2,334 µs | 2,865 µs |
| Host frame, BASIC loop | 42 µs | 49 µs |
| zexdoc, host | 32 s | 40 s |
| zexdoc, zexall | 67/67 each | 67/67 each |
| coretest | 1,356/1,356 | 1,345/1,356 |
| Machine against fuse-tspico | match | match |
| P4 RAM hash after 2,800 frames | `22bb9d65` (same as host) | `22bb9d65` (same as host) |
| `micropython.bin` | 0x1ccfe0 | 0x1bff70 |

DECISIONS.md D5 explains the 11 coretest differences. None of them shows in a TS2068 run. **Decided: Fuse's core (D5), and Gate 2 is met.** The board was left with the Fuse image.

## Phase 3 progress (2026-10-30)

**The panel works from our own code (`platform/p4_display.c`).** `ts2068.run(home, exrom, demo=False)` runs the machine in real time on core 0 and draws every frame on the panel. An esp_timer ticks at 60.1145 Hz and wakes the task. `demo=True` types the benchmark's BASIC loop.

- **Bring-up**, from Elecrow's V1.2 Lesson07 (repo cloned to `~/esp/ref/crowpanel-p4`, commit `f0ebc0c`, 5.5 GB, outside this repo):
  - LDO3 at 2.5 V for the DSI PHY.
  - DSI with 2 lanes at 900 Mbps.
  - The EK79007's start-up commands, the same as Espressif's `esp_lcd_ek79007` 1.0.2, sent by our own code with no managed component.
  - Backlight PWM on GPIO31 at 30 kHz, on LEDC timer 3, channel 7 (MicroPython's `machine.PWM` allocates from 0).
  - The panel reset is a software reset, as Elecrow does it.
  - `esp_lcd` and `esp_driver_ppa` are already in MicroPython's build; the usermod links them (`idf::esp_lcd`, `idf::esp_driver_ledc`, `idf::esp_driver_ppa`).
- **The DPI clock is 240 MHz divided by a whole number** (`mipi_dsi_hal_host_dpi_calculate_divider`). Elecrow asks for 51 MHz, gets 60 MHz, and so runs the panel at 65.8 Hz, which we measured. Espressif's 52 MHz preset also gets 60 MHz. We keep 60 MHz and Elecrow's porches, except a vertical front porch of 73 lines instead of 12: 1414 × 706, **60.10 Hz**, against the TS2068's 60.1145 Hz. The panel takes it. (We measured about 60 refreshes a second.)
- **Layout:** the 640×240 picture, every line doubled, at (0, 60). The 384-pixel side panel on the right showed the 16 TS2068 colours as bars (0–7, then 8–15, top to bottom), as a check of colour order and byte order; the keyboard has replaced them. RGB565 is written little-endian, which is what IDF's DPI driver expects. Elecrow's LVGL demo swaps bytes, but it draws only black and white, which look the same either way. **Checked by eye on the panel (2026-10-30):** the TS2068 screen and the colour bars are right, so little-endian RGB565 and the colour order are correct.
- **Three frame buffers in PSRAM** (IDF's maximum): one being scanned, one flipped and waiting for the next refresh, one to draw into. Drawing never waits for the panel and never tears. A frame drawn over before the panel showed it counts as `replaced`.
  - On this chip revision (before v3) there's no DSI vsync interrupt. IDF raises `on_refresh_done` from the DMA's end of transfer, exactly when the DMA restarts on the last flipped buffer, so "being scanned" is updated there.
- **Only changed lines are drawn.** `ts2068_render_line()` renders one line at a time into a 640-byte buffer, and each line is hashed. A line is converted only if the target frame buffer doesn't hold it already. If nothing changed since the frame shown, nothing is drawn.
- **The CPU never writes the frame buffers.** It converts changed lines to RGB565 in two internal-RAM strips of 16 lines. The PPA copies each strip to the frame buffer twice, treating it as 300 rows of 2048 pixels: once at x = 0 (the even panel rows) and once at x = 1024 (the odd rows). That doubles the lines with no scaling, so it's exact. `ts2068.display_check()` (after `stop()`) compares all 480 rows with the machine's screen: 0 pixels differ, in both runs below.

**Q9 answered: yes.** Per frame, on core 0:

| | Z80 | Render and draw | Worst frame (both) | Late ticks |
|---|---|---|---|---|
| First try: CPU converts into PSRAM, every line, two buffers | 3.0 ms | 2.0 + 11.2 ms (35% of frames dropped) | 17.0 ms | 0 |
| CPU into PSRAM, changed lines only, three buffers, full redraw forced | 3.1 ms | 2.1 + 11.2 ms | 17.6 ms | 7 in 20 s |
| **PPA strips, full redraw forced** | 2.5 ms | **6.5 ms** | **9.9 ms** | 0 |
| **PPA strips, normal (BASIC demo)** | 2.5 ms | 2.6 ms (6.4 ms when a frame changes) | 9.6 ms | 0 |

- The CPU writes PSRAM at only about 56 MB/s with the cache (write-allocate reads, plus the panel's 60 MB/s of DMA reads). The cache write-back was only 0.42 ms of the 11.2.
- Even when nothing changes, rendering and hashing all 240 lines costs about 2.4 ms a frame. Marking lines dirty when the Z80 writes the display file (Fuse's `display_dirty.c`) would remove most of that.
- The display still draws once a frame from screen RAM, so mid-frame changes to colours, modes or the border don't show (Q7).
- **Space:** `micropython.bin` is now about 0x1d4200 of 0x1f0000 (94%).
- **Gotcha:** adding a qstr that frozen modules already use (`core`, `run`) fails with `redeclaration of enumerator 'MP_QSTR_…'` in `frozen_content.c`. That file isn't regenerated when the usermod's qstrs change. Delete `build-…/frozen_content.c` and rebuild.

**Sound works, the same as Fuse's (DECISIONS.md D7).** `ts2068.run(home, exrom)` now has sound by default. `sound=False` (the fifth argument) uses the timer instead. `ts2068.volume(0–100)` sets the level; the default is 10 (50 was far too loud on the panel).

- **Fuse's sound for a TS2068**, lifted into `machine/sound/`:
  - Fuse gives the TS2068 no beeper of its own (libspectrum's TS2068 has no BEEPER capability), so its speaker type resolves to TV.
  - The ULA level from port FE goes through `ula_filter`: EAR (bit 4) is 50×256, and MIC (bit 3, active low) adds 2×256.
  - The AY goes into the TV buffer.
  - The sum goes through `tv_filter`, a band-pass from 100 Hz to 10 kHz. The output is mono.
  - `sound_ula` is called at the port FE write, `sound_ay_write` at the port F6 write, and `sound_ay_reset` at reset. `sound_frame` runs in the frame event after `tstates` is reduced, as in Fuse's `spectrum_frame`.
- **Checked sample for sample (2026-10-30):** `make -C test/machine sound` runs three ROMs in fuse-tspico (`--automation-capture-audio`) and in `run_ts2068 --audio`, at 44.1 and 48 kHz, for 300 frames.
  - The ROMs: the sound test (`test/basic/sound.asm`: a beeper sweep, MIC alone, AY tones, noise, four envelope shapes, sample playback by R8 writes, and the beeper with an AY tone), the stock ROM and the TS-Pico ROM.
  - Every sample matches.
  - The float build (`run_ts2068-float`, the P4's arithmetic) differs by 1 in 160 of 220,079 samples, and never by more.
- **Blip_Buffer's accuracy has to be 32 bits.** Fuse's `configure.ac` sets `BLIP_BUFFER_ACCURACY` to 32 wherever `long` is 64 bits, and leaves it at 16 otherwise. The reference build on macOS has 32.
  - At 16, the resampling factor rounds to 819/65536, so a frame makes 733.4 samples at 44.1 kHz instead of 733.6. That puts the samples at slightly different times, and nothing matched.
  - The lifted `blipbuffer.c` keeps resampled time in `uint64_t` and uses 32 on every platform, including the P4, where `long` is 32 bits. A Fuse built for a 32-bit platform would not match its own macOS build.
- **On the P4:** two NS4168 amplifiers on I2S (LRCLK 21, BCLK 22, DATA 23), one on each channel, enabled by GPIO30 low. The pins and the Philips 16-bit stereo format come from Elecrow's Lesson12.
  - `platform/p4_audio.c` writes each mono sample to both channels. The DMA buffer is 4 × 256 frames, about 23 ms.
  - `run()` with sound has no timer: it runs a frame, draws it, then writes the frame's samples, waiting for room. The I2S clock is the master.
- **Fuse's `double` filters cost 4.3 ms a frame on the P4,** whose FPU is single precision only. The filters and the mix now use `sound_real` (`machine/sound/sound_real.h`): `double` on the host, `float` in the P4 build. That brought the sound down to about 1.5 ms. The rest is the AY engine (1,834 steps a frame) and Blip_Buffer.

| Per frame, on core 0 | Z80 + sound | Display max | Worst frame | Frames/s | Underruns |
|---|---|---|---|---|---|
| Sound ROM, no sound (timer) | 2.7 ms | 7.1 ms | 10.2 ms | 60.04 | (0 late) |
| Sound ROM, `double` filters | 7.0 ms | 7.1 ms | 13.6 ms | 60.10 | 0 |
| **Sound ROM, `float` filters** | **4.2 ms** | 7.1 ms | **12.3 ms** | 60.10 | 0 |
| **Stock ROM, BASIC demo, 60 s** | **4.2 ms** | 7.4 ms | **12.0 ms** | 60.11 | 0 |

- The I2S clock divides down to 44,096 Hz, not 44,100, so frames run at 60.11 Hz, 0.01% slow. The panel's 60.10 Hz is close enough that no frames were replaced.
- **Space:** `micropython.bin` is now 0x1dea50 of 0x1f0000 (97%), leaving 69 KB. The next layer (touch, keyboard, storage) will need a bigger app partition.
- **Between runs (2026-10-31):** the I2S and the amplifiers are now off between runs (`p4_audio_start`/`p4_audio_stop`). Left running, the idle DMA counted every empty buffer as an underrun: over 4 million after a few hours. The stats now start again at each run.

**The on-screen keyboard works (2026-10-31).** `ts2068.run()` draws the 40 keys in the side panel and reads the touch panel every frame. Checked on the panel: typing, and CAPS SHIFT with a key, work.

- **Touch:** a Goodix GT911 at I2C address 0x5D, on SDA GPIO45 and SCL GPIO46, with reset on GPIO40 and interrupt on GPIO42 (Elecrow's Lesson05). Read back from the chip: product ID "911", firmware 0x1060, 1024×600, in the panel's own coordinates (no swap or mirror). A second device answers on the same bus at 0x2F; unidentified, left alone.
  - `platform/p4_touch.c` talks to it directly with IDF's new I2C master driver (which MicroPython also uses, so there is no legacy-driver conflict), on I2C port 1. No reset sequence: the chip comes up at 0x5D on its own.
  - Each read takes the status register (0x814E) and, when it has a new report, the touches from 0x814F, then clears the status. Up to five touches.
  - **Gotcha:** the I2C timeout is in FreeRTOS ticks, 10 ms each. A 5 ms timeout rounds to 0, and every transfer fails with `ESP_ERR_INVALID_STATE`. It's 50 ms now.
- **Keyboard:** `platform/p4_keyboard.c`. Four rows of ten keys, 38×90 pixels each, in the bottom 360 lines of the side panel; the 240 lines above are left for the OSD. Each touch presses the key under it, so shift combinations are two fingers. A key shows inverted while down.
  - Labels are drawn in the HOME ROM's character set, from 0x3D00 (found there in both the stock and the TS-Pico ROM), using columns 1–6 of each 8-pixel character so that "SYMBOL" fits. A ROM without a font there (the sound test ROM) gets garbage labels.
  - `p4_display` has a side-panel hook: it calls the keyboard for the buffer it's drawing into, and flips even when the TS2068's picture hasn't changed if the keys shown differ. The keyboard keeps which keys each of the three buffers shows, and redraws only those that changed.
- **Cost:** reading the touches takes 0.19 ms a frame on average, 0.57 ms at most. The first three frames draw the whole keyboard into each buffer (a worst frame of 18.9 ms, covered by the audio buffer: no underruns).
- **Space:** `micropython.bin` is 0x1df5e0 of 0x1f0000 (97%), 67 KB free.

**Our own board: a 4 MB app partition (2026-10-31).** The image had reached 97% of MicroPython's 1.94 MB app partition. `boards/CROWPANEL_P4_7` is MicroPython's `ESP32_GENERIC_P4` with the `PRE_REV3_C6_WIFI` variant, included from the port, plus its own `partitions.csv`. The resulting sdkconfig differs from the generic build's only in the partition table.

- **Build:** in `~/esp/micropython/ports/esp32`,
  `make BOARD_DIR=<repo>/boards/CROWPANEL_P4_7 USER_C_MODULES=<repo>/usermods/micropython.cmake -j8`. The build directory is `build-CROWPANEL_P4_7`.
- **Partitions:** nvs and phy_init as before; the app is 4 MB at 0x10000. MicroPython adds `vfs` from 0x410000 to the end of the flash, 11.9 MB (it was 14 MB). The image is 0x1df5c0, leaving 2.1 MB (53%).
- **The partition path:** sdkconfig names the table relative to the port directory, which can't reach a board outside it. `mpconfigboard.cmake` writes the absolute path into a sdkconfig fragment at configure time; IDF takes absolute paths as they are.
- **Moving to it wipes the filesystem,** since `vfs` now starts at a different place. Done once: copied the 36 files (298,612 bytes) to `backups/vfs-2026-10-31/` with `mpremote cp -r :/ .` (1.5 minutes), flashed `partition_table/partition-table.bin` at 0x8000 and the app at 0x10000 (the bootloader is unchanged), and copied them back. Every file's SHA-256 matched the backup.
- **Flash from now on:**
  `esptool --chip esp32p4 --port /dev/cu.wchusbserial10 --baud 230400 write-flash 0x10000 build-CROWPANEL_P4_7/micropython.bin`
  (add `0x8000 build-CROWPANEL_P4_7/partition_table/partition-table.bin` on a board still on the old table).

**The microSD card (2026-10-31):** about 1 GB, FAT, holding only `TAP/` from the trace seed (`seed_sd.py`). `machine.SDCard` with `/p4/sd.json` (slot 0, 1-bit, 10 MHz) opens it in 37 ms.

**ROMs and settings from the card (2026-10-31).** `import emu; emu.start()` boots the TS2068 from the card's `TS2068` folder.

- **On the card:** `/TS2068/settings.json` (`rom`, `exrom`, `volume`, `sound`; missing keys take `emu.DEFAULTS`: the stock pair, volume 10, sound on) and `/TS2068/ROMS/`. A `rom` of 32K holds HOME then EXROM, as `TSPICO-23.ROM` does, and `exrom` is then ignored.
- **The TS-Pico's card too:** the TS-Pico firmware lists only `/TAP` (David Anderson, 2026-10-31), so the `TS2068` folder never shows in its menus. `emu.py` mounts the card only while it reads, and unmounts it before the machine starts, so the firmware can mount it itself.
- **Getting ROMs there:** `python3 tools/p4/sd_roms.py [FILE ...]` mounts the card through mpremote and copies the files to `/TS2068/ROMS`; with no files, the stock pair and `TSPICO-23.ROM`. mpremote's chained commands need `+` between them to share one session (and so the mount). `tools/p4/install.py` now puts `emu.py` on the flash.
- **Checked on the panel:** `emu.start()` reads the settings and ROMs in 0.46 s, and both the stock pair and the TS-Pico ROM run (no late frames). The first run after a boot had one underrun at start-up; the second had none.
- `emu.save(**changes)` updates `settings.json`, `emu.settings()` reads it, and `emu.roms()` lists `ROMS/`.

**Snapshots (2026-10-31).** SZX, as libspectrum writes it for a TS2068, so Fuse loads ours and we load Fuse's (`machine/szx.c`).

- **The file:** header `ZXST` 1.5, machine 12 (TS2068), then Z80R, SPCR, JOY (Timex 1 and 2), KEYB, RAMP pages 5, 2, 0 (HOME RAM 0x4000, 0x8000, 0xC000), AY, SCLD and ZXPR, in libspectrum's order. Ours are uncompressed, 49,327 bytes. libspectrum and Fuse compress pages with zlib by default (about 2 KB for a booted ROM); we load either, with zlib on the host and `tinfl` from the P4's ROM (a static 11 KB decompressor, since `tinfl_decompress_mem_to_mem` puts it on the stack).
- **What's restored, in Fuse's order** (`snapshot_copy_from`): a soft reset, port FE at T-state 0 (so the ULA's sound level), the T-state, the Z80 with MEMPTR and the EI-last, halted and F-set flags, port F4, then port FF, then the AY registers as stored (unmasked, as Fuse does) and to the sound at time 0. Not in SZX, and not restored by Fuse either: the frame count, the FLASH phase, Z80 events pending at the save, and `iff2_read`.
- **The core interface** gained `ei_last` and `f_set` in `z80_regs_t` (Fuse's `interrupts_enabled_at == tstates` and Q). Z80_JLS carries `ei_last` (its pending EI) but not Q, which it keeps private. Its coretest output is unchanged.
- **DOCK:** a snapshot with DOCK-bank pages is refused, since this machine has no DOCK; EXROM-bank pages are skipped, since the EXROM comes from the configuration (Fuse writes them only with a DOCK).
- **Checked on the host:** `make -C test/machine szx` runs `tools/host/compare_szx.py`. For the sound, stock and TS-Pico ROMs, snapshots at frames 32, 160 and 320:
  - libspectrum (`szx_resave`, linked against `~/esp/ref/prefix`) reads ours and writes it back, uncompressed, **byte for byte the same**;
  - ours loads libspectrum's compressed copy, Fuse loads ours (`--snapshot`), and both run 100 frames: the registers, the screen and every sound sample match, and match ours run straight through.
  - The frame counts are multiples of 32, because the FLASH phase isn't in the file.
- **Two fuse-tspico automation bugs** turned up; neither touches emulation:
  - Its result reports R as `(z80.r & 0x7f) | z80.r7` (`automation/state.c:50`) without masking r7 to bit 7. After a snapshot load r7 holds the whole saved byte, so the reported R is wrong (0x6F ORed in, here). Never seen from a cold boot, where r7 is 0. `compare_szx.py` applies the same formula to our R before comparing.
  - The Timex PNG writer (the one `compare_fuse.py` already works around) reads past its buffer and crashes Fuse (SIGSEGV or SIGBUS in `get_rgb32_data`) about half the time when sound is on, cold boot or not. `compare_szx.py` takes the screen from a `--no-sound` run and the sound from a run with no screenshot.
- **On the board:** `ts2068.snapshot()` and `ts2068.load_snapshot(data)`; while the machine runs, the run task does them between frames. Loading libspectrum's compressed file on the board, then saving, gives the same bytes as the host doing the same. Load 7.0 ms (with inflate), save 3.1 ms.
- **On the card:** `emu.save_snapshot(name)` writes `/TS2068/SNAPS/name.szx` from the running machine in 0.30 s; `emu.load_snapshot(name)` loads it in 0.14 s, starting the machine first if it isn't running. No late frames or underruns either way. libspectrum rewrites the board's file identically.

**The side-panel menu (2026-10-31).** `import osd; osd.start()` (`usermods/ts2068/osd.py`) starts the machine if need be and puts a menu in the 240 lines above the keyboard: Load ROM, Load snapshot, Save snapshot, Reset, and Volume - / +.

- **C:** `platform/p4_osd.c` draws ten rows of 31 characters in the ROM's font at twice its size (white on black, black on white for titles, white on grey for buttons) and reports taps as (row, column). It shares the side-panel hook with the keyboard and, like it, redraws only the rows a frame buffer doesn't yet hold. The run task passes it the touches each frame.
- **MicroPython:** `ts2068.osd(row, text, style)` and `ts2068.osd_tap()`. `osd.py` polls the taps every 30 ms on a thread of its own, and does the work (card, restart) there, so the machine keeps running.
- **Load ROM** lists what can boot: 32K files, and 16K `*-0.*` HOME ROMs whose `*-1.*` EXROM is on the card. The one picked becomes the setting and the machine restarts on it. **Save snapshot** writes `SNAPnnn.SZX`, the next number free. **Volume** steps by 5 and is saved. `emu.start()` now stops a running machine first, so it is also the reset.
- `mpremote exec` soft-resets the board first, which ends the menu thread (the machine, a C task, keeps running): start it with `mpremote ... resume exec "import osd; osd.start()"`, or from the REPL.
- **The card was turning the touch panel and the screen off.** The first try on the panel came up black, with touch failing every read. MicroPython's `SDCard` takes the P4's LDO4 (`MICROPY_HW_SDMMC_LDO_CHAN_ID`) and releases it at `deinit()`. IDF switches an LDO off when its last user releases it, and LDO4 feeds the VDDPST_5 pad domain, GPIO39-48: the card, but also the GT911's I2C (45/46), its reset and INT (40, 42), and the LCD's reset (41). So every `emu` card access cut them. A software reset didn't bring touch back. After a power cycle the GT911 answered at 0x14, not 0x5D (it takes its address from INT at the end of reset, and nothing was driving INT). Fixes: the card is opened with `ldo=None` (`tools/p4/board.json`), so MicroPython leaves LDO4 alone (the pads are powered from boot). `p4_touch_init` holds LDO4 on at 3.3 V, resets the GT911 with INT low for 0x5D, and tries 0x14 if that fails. After that, touch had no errors through card reads, a reset and a snapshot save.

## Phase 4 progress (2026-10-31)

**The TS-Pico is plugged into the running machine.** `emu.start()` with a 32K ROM (the TS-Pico's, `TSPICO-23.ROM`) now starts the firmware (`p4host.start()`, the first time only; READY in 2.7 s) and runs the machine with `ts2068.run(..., tspico=True)`, the sixth argument. That points `machine/ts2068.c`'s existing port hook (`config->tspico`, ports 0Eh/0Fh) at `p4bridge_frame()`, so the Z80 on core 0 talks to the firmware on core 1 through the same queues Gate 1 replayed traces through. Fuse's `tspico.c` needed no porting: its socket and HELLO are the parts in-process doesn't need. `"tspico": false` in `settings.json` leaves it unplugged.

- **Checked on the panel:** `LOAD ""` typed on the on-screen keyboard loads from the TS-Pico. 18,255 bridge frames, no bridge underruns or overruns, no late frames.
- **The card:** the firmware keeps one `SDCard` for good (p4host) and the slot takes one at a time, so `emu`'s `_Card` mounts the firmware's card when there is one, rather than opening its own.
- **The whole panel blinked during a load, and the sound broke up: flash writes.** The firmware writes to the flash filesystem as it works (its log, `TMP`). A flash write turns the caches off. IDF's DSI driver restarts the panel's DMA from an interrupt at the end of every refresh, and by default that interrupt waits for the caches, so the panel went blank until the write ended (a phased test on the panel: blinks only while writing flash, not idle or on SD). And core 0 couldn't fetch the emulator's code either: 5 s of flash writes gave 14 audio underruns and a 52 ms frame. Fixes, in `boards/CROWPANEL_P4_7/sdkconfig.board`: `CONFIG_LCD_DSI_ISR_CACHE_SAFE` (our refresh callback was already in IRAM) and `CONFIG_SPIRAM_XIP_FROM_PSRAM`, which runs the code from a copy in PSRAM so flash writes leave the caches on. After them: no blinking, and no underruns or long frames from flash writes. The emulator is as fast as before (4.1 ms a frame); the image grew 3.4 KB, and 27.8 MB of heap is left.
- **What's left of the stalls is the firmware's own pace.** `IN (0Eh)` blocks until the firmware queues the byte (up to 50 ms), which freezes emulated time. `ts2068.run_status()` now counts these (`pico_waits` over 1 ms, `pico_wait_us`, `pico_wait_max`). A `LOAD ""` (`tools/p4/tspico_load.py`, which types it through the new `ts2068.keys()`) waits about 40 times, 250 ms in all and at most 18 ms, mostly in its first 300 ms, which leaves 12 audio underruns: a gap of silence while the Z80 has nothing to do but wait. No buffer covers a quarter of a second, so this stays until the firmware answers faster.
- **Not the SCLD:** while looking for the blink, the ROM's port FF and F4 writes during a load were logged. They only page the EXROM (DEC 00/80, HSR 00/03), and the screen RAM is cleared once, as the load starts.

**Gate 4, fuse-smoke: the same bytes (2026-10-31).** The fuse-smoke session (`LOAD ""` of the test program in `/assets/nofile.tap`, which runs `CAT`, mounts `tpi:fusedata.tap`, `LOAD "" CODE 40000` and `VERIFY`) run on the live machine, the Z80 being ours and the keys typed through `ts2068.keys()` (`tools/p4/capture_load.py`), gave the same bus frames as fuse-tspico + `pico_host`: 2,451 of 2,451, compared by `tools/host/compare_frames.py --timing`. `ts2068.pico_trace(n)` keeps every frame the hook passes. Compared strictly, two things differ, and both are timing:

- **The session id**, bytes 3-4 of each LOAD/SAVE pre-header, and so its XOR, byte 9. The ROM takes it from FRAMES (+1, never 0: the firmware's `docs/reference/appendix/glossary.md`), so it depends on how long the machine had been on: `7E` here against Fuse's `55` in the first.
- **Status changes the Z80 saw.** The trace has a BUSY-READY pair (`IN0F 00 -> 00`, `-> FF`) after some pre-headers that our run never caught: the trace logs a status read only when it changes, and our Z80 polled at different moments. 2,473 frames to 2,485 before these are left out.

Before the run, three files the firmware had written in earlier runs were moved out of the seed's way, since its replies depend on what's there: `/TMP/temp.tap` and `/activity.log` (to `/gate4-backup/` on the flash) and `/sd/TAP/dirinfo.tap` (to `/sd/TS2068/gate4-backup/`). The `TS2068` folder stayed on the card: the firmware lists only `TAP`. zesarux-smoke (SAVE, the printer, the `tpi:cd` menu) is still to do: its seed needs the card's `help` folder and an empty `TAP/GAMES`, and its keys are many more.

**Gate 4 met: zesarux-smoke too (2026-10-31).** The firmware's `tools/emu/smoke.py`, step for step, on the live machine (`tools/p4/zesarux_smoke.py`): CAT, mount and LOAD a BASIC TAP and LIST it, `SAVE "saved"` (pressing a key at "press any key"), `tpi:info`, the virtual printer (`tpi:picopt`, `LPRINT`), and the `tpi:cd` menu cancelled with n and used with 0. All 11 of smoke.py's checks pass, and the 2,095 bus frames match ZEsarUX + `pico_host`'s with `compare_frames.py --timing`. Re-checked, fuse-smoke's capture still matches.

- **How it types:** `tools/p4/session.py` (copied to `/session.py`) does what the firmware's `session.py` does to ZEsarUX: placeholder keys, then the tokenised line written into the edit line, ERR_NR set to FFh, ENTER, and wait for a report or some text. New in `ts2068`: `peek(addr, n)` and `poke(addr, data)` on HOME RAM, which core 1 does while the machine runs, as a debugger would, and `screen_text()`, the display file read back as text by matching each cell against the ROM's font. The editor drops a key that comes while it's busy with the last one (9 of 16 arrived the first time), so `run()` counts what reached the edit line and tops it up.
- **The session id again:** a SAVE's header and data blocks echo it after their flag (`OUT 00, SESSION lo/hi, 17 header bytes, CRC`: the firmware's `docs/PROTOCOL.md`; the CRC leaves it out), so `--timing` masks that copy too.
- **The card and flash for it** (agreed with the user): the firmware's `SD card/help` at `/sd/help`, `hello.tap` and an empty `GAMES` in `/sd/TAP`, and the stock `/assets/nofile.tap`. Everything they replaced, and what each run wrote, is in `/gate4-backup` (flash) and `/sd/TS2068/gate4-backup` (card), suffixed `.2`, `.3`, `.4` by run; nothing was deleted. Two runs failed on the way (the dropped keys, then the script reading `saved.tap` as text), and the files they wrote were moved aside before the next, since the firmware's replies depend on what's there.

**Boot to the TS2068 (2026-10-31).** `main.py` (`usermods/ts2068/main.py`, installed by `tools/p4/install.py`) runs `emu.start()` and `osd.start()` at power-on: the machine from the card's settings, the TS-Pico firmware too for a 32K ROM, and the menu. Checked from a hard reset: the TS-Pico ROM's copyright line on screen, the firmware READY, the menu running, no late frames. `/p4/noboot` on the flash skips it (checked). It waits a second first, so mpremote's Ctrl-C can stop a start that goes wrong. After a soft reset (Ctrl-D at the friendly REPL) it starts the machine afresh, since the reset ends the menu's and firmware's threads but not the machine's task; mpremote's own soft resets enter the raw REPL, where main.py doesn't run, so they still leave the machine running without its menu or firmware.

