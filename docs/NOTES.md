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
`make BOARD=ESP32_GENERIC_P4 BOARD_VARIANT=PRE_REV3_C6_WIFI USER_C_MODULES=<repo>/usermods/micropython.cmake` in `~/esp/micropython/ports/esp32`. App 1,776,016 bytes (255,600 left). FreeRTOS tick is 100 Hz (`CONFIG_FREERTOS_HZ=100`), and the idle-task watchdog is off on both cores, so a spinning task on core 0 is allowed. A user module's qstrs must not clash with frozen ones: a dict key named `n` broke the build (`redeclaration of enumerator 'MP_QSTR_n'`).

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

