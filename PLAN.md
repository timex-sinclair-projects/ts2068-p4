# TS2068 + TS-Pico on the CrowPanel Advanced 7" ESP32-P4: Project Plan

A standalone Timex/Sinclair 2068 with a TS-Pico, running on an Elecrow CrowPanel Advanced 7" ESP32-P4 HMI display.

This is **our own emulator**: a new P4 platform layer and project structure, with the TS2068 machine and Z80 core lifted as C modules from existing GPL emulators, and the **real** TS-Pico MicroPython firmware running beside it. It is not a port of any single emulator.

The ZX Spectrum is out of scope. The only target machine is the TS2068 with a TS-Pico.

*Revision 2 (2026-10-25): moved from "port Fuse" to "own project, borrowing modules"; added a Phase 2 comparison of Z80 cores; closed Q8 (licensing).*

---

## 1. Architecture

### 1.1 Runtime: two cores, one image

On the desktop, fuse-tspico and zesarux-tspico don't emulate the TS-Pico. They forward every Z80 access to ports 0Eh/0Fh, as a 2-byte bridge frame over a socket, to `pico_host`. `pico_host` runs the real firmware under CPython with stand-in modules for the RP2040 parts. On the P4 the same split happens inside one image:

- **Core 0:** the emulator (Z80, TS2068 machine, platform layer).
- **Core 1:** MicroPython running the TS-Pico firmware (`src/TS`), with replacements only for the parts specific to the RP2040.
- **Between them:** a pair of in-memory queues carrying bridge-v1 frames, in place of the socket.

```
┌───────────────────────── ESP32-P4 ─────────────────────────┐
│ Core 0: emulator                │ Core 1: MicroPython      │
│  Z80 + TS2068 machine           │  src/TS firmware         │
│  tspico hook ─[op,value]─► queue ┼─► BusModel (rx)          │
│              ◄─reply byte─ queue ◄┼── BusModel (tx / status) │
│  platform: MIPI, I2S, touch,    │  SD: board microSD       │
│  keyboard, microSD              │  (same TAP card layout)  │
└─────────────────────────────────┴──────────────────────────┘
```

### 1.2 Source: four layers, each with a single source

| Layer | Source | Notes |
|---|---|---|
| **P4 platform:** MIPI-DSI display, I2S audio, touch, keyboard, microSD, task and core setup | **Written new.** No existing project has it. | The largest part of the work. |
| **TS2068 machine:** SCLD screen modes, 60 Hz frame timing, port F4 dock/EXROM paging, 16K EXROM, AY on F5/F6, joysticks via AY port A | **Lifted from Fuse** (`machines/ts2068.c`, `machines/tc2068.c`, `peripherals/scld.c`, memory paging), at the fuse-tspico tag | fuse-tspico already has the 16K EXROM and the 0Eh/0Fh hook. Its TS2068 **timing** is Spectrum-derived and must be checked (§1.6). |
| **Z80 core** | **Decided in Phase 2:** Fuse's core vs ESPectrum's Z80_JLS | Choose by speed on the P4, then accuracy on the test suites. |
| **TS-Pico** | The real firmware from `tspico-firmware-build` | Adapted for the P4 through an upstream platform layer (§1.3). |

An emulator core boundary inside our tree (`machine_*` API: `run_frame`, `io_read`/`io_write`, `mem_*`, `frame_sink`, `audio_sink`) keeps the lifted code separate from the platform. The Z80 core and the machine can then each be swapped without touching the platform layer.

### 1.3 TS-Pico firmware: a platform layer upstream, not shims maintained here

`pico_host` adapts the firmware by installing stand-in modules (`rp2`, `machine`, the SD driver) under it. That works, but every firmware change can break the stand-ins silently. Since the firmware is our own project, the better path is upstream:

- Add a small platform-abstraction module to `src/TS`, roughly a `TS/platform.py` that picks the bus and SD implementations. Its backends are `rp2040` (the real board), `host` (what `pico_host` does today) and `p4` (this project).
- Move the `BusModel` semantics from `pico_host.py` into a shared module, so the desktop host and the P4 run the same model.
- Document the in-process transport in `EMULATOR_BRIDGE.md` (proposed as v1.1 or v2).

Until that lands, the P4 can use `pico_host`-style stand-ins as a temporary bridge. Phase 1 does that on purpose, so the upstream change is shaped by a working prototype.

### 1.4 What was ruled out and why

- **Porting a whole emulator** (Fuse, ZEsarUX or ESPectrum): we'd carry their UI systems, build systems and desktop assumptions in exchange for two modules we actually need.
- **ESPectrum as the base:** it has no TS2068 support, and it targets only the classic ESP32 (every sdkconfig says `CONFIG_IDF_TARGET="esp32"`). Its achievements, fitting a Spectrum into a 240 MHz Xtensa with no PSRAM and generating VGA from ESP32 peripherals, don't carry over to the P4. Kept as a source of techniques (§1.5) and as a Z80 core candidate.
- **TSRun as the base:** browser-only JavaScript with **no license**, an 8K EXROM only and no TS-Pico support. Kept as a TS2068 behavior reference through its README; read as documentation, no code copied. A separate browser TS-Pico effort is noted in §6.
- **Wi-Fi bridge to `pico_host` on a PC:** a network round trip on every port access. Kept as a development aid only.
- **Firmware rewritten in C:** two firmwares that drift apart. Ruled out.

### 1.5 Lessons to take, by source

- **ESPectrum:** keeping hot emulator state in internal RAM; generating video in step with the raster on a microcontroller; its frame and audio sync loop; on-screen OSD and file-browser design for a board with no host OS.
- **Fuse:** the TS2068/TC2068 machine and SCLD; the periph/module registration pattern; the debugger and `coretest.c` for checking the Z80.
- **ZEsarUX:** zesarux-tspico is a second working bridge client. It is **not** an independent TS2068 timing reference: its contention model is openly copied from the 48K (§1.6).
- **TSRun (documentation only):** a TS2068 raster model at 224 T-states per line × 262 lines = 58,688 T-states per 60.1145 Hz frame, with the beam sampling the border, the port FF mode and memory as it goes. Also its SCLD and AY audio mix modelled on schematic values, and its 60.1145 Hz audio-paced frame loop. Each of these is a claim to check against hardware or Fuse before relying on it.

### 1.6 Fuse and ZEsarUX model the TS2068 as a Spectrum with extras

Both emulators were written by Spectrum specialists, and their TS2068 timing comes from the Spectrum, not from the SCLD:

- **Fuse** uses the Spectrum ULA's delay table (`spectrum_contend_delay_65432100`) for the TS2068, and marks DOCK and EXROM pages contended in 0x4000–0x7FFF. The second part is wrong (§3, "Contention").
- **ZEsarUX** (`src/contend.c` in zesarux-tspico) says it outright: `//TODO timex. Hacemos como 48k` ("TODO timex. We do it like the 48K"). Its pattern is `65432100` starting at T-state 14335, which is the 48K Spectrum's first contended T-state. It contends by **address** (`(addr & 0xC000) == 0x4000`), regardless of what is paged in, so DOCK RAM at 0x4000 is contended there too. Ports with the address pattern of that range are also contended Spectrum-style.
- **ZEsarUX also offers the Spectrum "snow effect" on the TS2068.** The option is shown when `MACHINE_IS_SPECTRUM` (`current_machine_type < 40`), and the TS2068 is machine ID 17. Snow is the corrupted pixels on some Spectrum models when the I register points into 0x4000–0x7FFF, so refresh cycles hit contended memory. According to David Anderson, the 2068 shows no contention artifacts, partly because the SCLD stops the Z80 clock instead of only stretching memory accesses. Fuse has no snow emulation at all.

The two emulators agreeing therefore isn't evidence: they share an origin. The Spectrum's timing was measured on real machines by its community over decades, and as far as we know nothing comparable has been published for the SCLD. **Measuring it is a project deliverable**, with the results published to the TS2068 Ref Library and sent upstream to both emulators:
- Contention pattern and which T-states in a line are contended (Q10a)
- When and why the SCLD stops the Z80 clock (Q10b)
- I/O contention (Q10c)
- Whether stopping the clock also covers refresh and no-MREQ cycles, which would explain why there's no snow (Q10d)
- Frame timing: T-states per line, lines per frame, and the T-state of the first contended access

---

## 2. Known facts (verified)

### Target board: CrowPanel Advanced 7" ESP32-P4 (checked 2026-10-10)
| Item | Value |
|---|---|
| SoC | ESP32-P4NRW32: dual-core RISC-V up to 400 MHz (HP), plus a single-core LP core up to 40 MHz. **Chip revision v1.3** on our board (esptool, 2026-10-30): pre-revision 3, so MicroPython's `PRE_REV3` variants and `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` |
| USB | One USB-C to a CH340 (`1A86:7522`) on the P4 UART, for flashing; one USB-C to the P4's high-speed OTG port (checked 2026-10-30) |
| Memory | 32 MB PSRAM, 16 MB flash, 768 KB HP L2MEM |
| Display | 7" IPS 1024×600, MIPI-DSI, capacitive touch up to 5 points |
| Radio | ESP32-C6-MINI-1 co-processor (Wi-Fi 6, BT 5.3/BLE). The P4 has no radio of its own. |
| I/O | dual USB-C, UART, I2C, GPIO, microSD, speaker, battery, camera (optional variant) |
| Frameworks | ESP-IDF, Arduino |

Sources: Elecrow wiki, CNX Software (Oct 2025), OpenELAB listing, atomic14 board summary.

### fuse-tspico (checked 2026-10-10 and 2026-10-25)
- Release `tspico-v1.10.0-1` is at commit `a43958d` (2026-10-04). The TS-Pico work is on branch `tspico-device`.
- `peripherals/tspico.c` is 313 lines. The transport lives entirely in `tspico_send()`, `tspico_recv()` and `tspico_connect()`.
- The 16K EXROM is supported. `TSPICO-21.ROM` is 32K: a 16K HOME ROM followed by a 16K EXROM.
- Code to lift, with line counts: `machines/ts2068.c` 137, `machines/tc2068.c` 224, `peripherals/scld.c` 401; `z80/` is about 1,700 lines plus opcode files generated at build time from `.dat` by Perl. Generate those on the host and check them in.
- The license is **GPL-2 or later**: all checked headers (`z80/*.c`, `machines/ts2068.c`, `memory_pages.c`, `peripherals/tspico.c`) say "any later version".

### ESPectrum (`EremusOne/ZX-ESPectrum-IDF`, checked 2026-10-25)
- GPL-3, C++, last commit 2025-11-17.
- Classic ESP32 only. No TS2068 or Timex code.
- Z80 core: `src/Z80_JLS.cpp`, 6,521 lines.

### TSRun (`josef-jelinek/TSRun`, checked 2026-10-25)
- Browser JavaScript, about 10,400 lines; first commit 2026-08-30, mostly by a single author.
- **No license file**, and the README doesn't mention one. Treat it as all rights reserved.
- `exRomSize = 8192`. No 0Eh/0Fh hook.

### TS-Pico firmware (`timex-sinclair-projects/tspico-firmware-build`)
- **The code is MIT**, the docs are CC BY 4.0.
- `src/TS/` has about 12,000 lines of MicroPython (556 KB). Modules it imports: `rp2`, `machine`, `utime`/`time`, `micropython`, `_thread`, `uctypes`, `gc`, `os`, `json`, `array`, `select`, `errno`, `math`, `random`, `sys`.
- Native code specific to the RP2040: `src/tsbus/tsbus.c` (889 lines) and `tsbus.pio` (173 lines).
- `tools/emu/pico_host.py` (604 lines) contains `BusModel` (the model of PIO SM0 `TS_IO_DUAL`), `HostFS`, `micropython_env()` and `install_firmware()`.

### Bridge v1 (`docs/EMULATOR_BRIDGE.md`)
| Op | Z80 access | Value sent | Reply |
|---|---|---|---|
| 0 | `OUT (0Eh)` | byte written | ignored |
| 1 | `IN (0Eh)` | 0 | byte the Z80 reads |
| 2 | `IN (0Fh)` | 0 | status byte |
| 3 | `OUT (0Fh)` | byte written | ignored |
| 4 | HELLO | 1 | 1 |

- Only the low address byte is decoded, and only on the TS2068.
- Status values: `FFh` ready and idle, `F7h` command open, `FBh` TS-Pico reset, `00h` busy.
- No device present: `IN (0Fh)` returns `FFh` and `IN (0Eh)` returns `00h`.
- The bridge doesn't model bus timing. FIFO underrun and overrun are hardware-only tests.

### MicroPython on the P4
- MicroPython **v1.27** added ESP32-P4 support. The official board is `ESP32_GENERIC_P4`.

### Licensing outcome
- MIT (firmware) + MIT (MicroPython) + GPL-2+ (Fuse) + GPL-3 (ESPectrum Z80_JLS, if chosen) combine as **GPL-3 overall**.
- If the Fuse Z80 core is chosen, the project can stay GPL-2+.
- TSRun code may not be used.

---

## 3. Open questions: unverified until a test or primary source settles them

| # | Question | Blocks | How to check |
|---|---|---|---|
| Q1 | Can one ESP-IDF image run MicroPython on one core and native C on the other? | Everything | Phase 1 |
| Q2 | Which Z80 core runs the TS2068 at full speed on a 400 MHz P4 core, and with how much headroom? | Phase 3 | Phase 2 |
| Q3 | Can either USB-C port act as a USB host for a keyboard? | Keyboard choice | Schematic, Elecrow wiki, test |
| Q4 | Does a BLE HID keyboard work through the C6 (esp-hosted)? | Keyboard choice | esp-hosted docs, test |
| Q5 | Does the firmware use `machine.SDCard` or its own SD driver (`TS.sdcard`)? | Phase 4 SD mapping | Read `src/TS/sdcard.py` and its callers |
| Q6 | Which Fuse files and generated files does the TS2068 machine pull in beyond those listed? | Phase 3 build | Lift-and-compile in Phase 2 |
| Q7 | How does Fuse render TS2068 hi-res (512-pixel) mode, and does it follow the beam? | Phase 3 display | Read `display.c`; compare with TSRun's documented raster model |
| Q9 | Is a PSRAM DMA framebuffer on the MIPI-DSI path fast enough while the Z80 is running? | Phase 3 display | Measure in Phase 3 |
| Q10 | Does Fuse's contention model match the SCLD? (a) Is the delay pattern the same as the Spectrum ULA's `6,5,4,3,2,1,0,0`? (b) Which SCLD activities stop the Z80 clock, and at which T-states in a line and in a frame? (c) Is I/O contended as well, and which ports? (d) Since the SCLD stops the Z80 clock outright, does that stall refresh and no-MREQ cycles too, so the Spectrum's per-cycle `no_mreq` delays don't apply? Test: point I at 0x40–0x7F on a real 2068 and confirm no snow. | Accuracy (timing-sensitive loaders, demos, raster effects) | Timex/SCLD documentation (David is locating it), Hildreth documents, then timing tests on a real TS2068 |

Q8 (licensing) is closed: see §2, "Licensing outcome".

### Contention: what's known so far (2026-10-25)
- **Hardware** (stated by David Anderson, TS-Pico developer): the SCLD controls access to the lowest 16K of motherboard RAM, chunks 2–3 (0x4000–0x7FFF). Every Z80 read or write to those chunks goes through the SCLD and is slightly slower than accesses to the upper chunks. The SCLD also controls the Z80's clock and stops it outright while it does certain activities. The exact activities are still to be documented.
- **Hardware, DOCK and EXROM** (stated by David Anderson): RAM from the DOCK or any other bank does **not** go through the SCLD, so it isn't contended even when it's mapped into chunk 2 or 3. That's what lets the SCLD keep generating video from HOME RAM while DOCK RAM is mapped over it. Software that does this has to move the stack and system variables first, which is why the Technical Manual advises against it.
- **Fuse** (`machines/ts2068.c`, `tc2068.c`, `peripherals/scld.c`): models contention on 0x4000–0x7FFF, with HOME RAM 5 contended and RAM 2 and 0 not. The delay table reuses the Spectrum's `spectrum_contend_delay_65432100`, which is the part Q10(a) questions.
- **Fuse bug (TS2068):** `scld_set_exrom_dock_contention()` in `peripherals/scld.c` deliberately sets `contended = 1` on the DOCK and EXROM pages for 0x4000–0x7FFF ("contended in home, Dock and Exrom in the 0x4000 - 0x7FFF range"). It's called from `tc2068.c`, from SCLD reset, and from `dck.c` on cartridge insert and eject. According to the hardware facts above, that's wrong for the TS2068: only HOME RAM in chunks 2–3 should be contended. The same function is also called from `spec_se.c` (Spectrum SE), which is out of scope, so leave that path unchanged.
  - **Our fix:** in the lifted machine module, DOCK and EXROM pages are never contended on the TS2068.
  - **Upstream:** send the same fix to fuse-tspico, and to Fuse itself, with the Technical Manual section as the citation, since Fuse's comment shows the current behavior was intended.
  - **Test:** a machine-code program that pages DOCK RAM into chunk 2 or 3 through port F4, times a fixed loop in it, and compares that time with the same loop in HOME RAM at 0x4000 and with upper RAM. On a real TS2068 the DOCK loop should match upper RAM. On unpatched Fuse it will match HOME RAM.
- **Hardware, artifacts** (stated by David Anderson): unlike some Spectrum models, the 2068 shows no visual artifacts from contention, partly because the SCLD stops the Z80 rather than only stretching its memory accesses. The emulator must never produce Spectrum snow on the TS2068.
- **For this project:** contention stays inside the lifted machine module, which keeps the `contended` page flags and the delay table. A Z80 core chosen in Phase 2 must support a per-access contention hook. Z80_JLS has to be checked for that before Phase 2 treats it as a candidate.

---

## 4. Phases

Each phase ends in a **gate**: a pass/fail result that decides whether to continue.

### Phase 0: Set up the workspace
- [x] Create the project repo (2026-10-30; submodule pins in `third_party/PROVENANCE.md`). Layout:
  ```
  /main            app_main, task and core setup
  /platform        p4_display, p4_audio, p4_touch, p4_keyboard, p4_storage
  /machine         TS2068 machine + SCLD + paging (lifted from Fuse)
  /z80             the chosen core (Phase 2)
  /tspico          tspico hook (C side of the bridge) + p4bridge queues
  /firmware        submodule: tspico-firmware-build (pinned)
  /third_party     fuse-tspico, ESPectrum, libspectrum (read-only references)
  /docs            DECISIONS.md, NOTES.md
  /test            trace fixtures, Z80 test suites, BASIC test programs
  ```
- [ ] Install the ESP-IDF version that MicroPython's P4 port requires. Read it from `ports/esp32/README.md`; don't guess. (Read 2026-10-30: v1.29.0 recommends ESP-IDF v5.5.2; see `docs/DECISIONS.md` D3. Chip revision is v1.3, so the variant is `PRE_REV3_C6_WIFI`: `docs/NOTES.md`.)
- [ ] Build the stock `ESP32_GENERIC_P4` MicroPython and flash it to the board to confirm the REPL works.
- [ ] Build fuse-tspico on the desktop and confirm the TS-Pico works against `pico_host`. **This is the reference implementation.**
- [ ] Record **reference traces** with `TSPICO_TRACE=1`: boot, catalog the SD card, load a TAP, save a TAP. Keep them as fixtures.

**Gate 0:** both reference systems work, and the trace fixtures exist.

### Phase 1: Prove the two-runtime design (main risk, Q1)
- [ ] Choose the integration approach and record it in `docs/DECISIONS.md`:
  - **A.** Build inside MicroPython's ESP32 port, with the emulator as a native C module (USER_C_MODULES) whose task is pinned to the other core.
  - **B.** Embed MicroPython (the `ports/embed` approach) inside an ESP-IDF application that owns `app_main`.
  - Start with A. Switch to B if A blocks core pinning or memory placement.
- [ ] Write `p4bridge`, a native module with two FreeRTOS queues:
  - C side: `bridge_frame(op, value) -> reply`, which blocks the calling task until the reply arrives.
  - Python side: the calls `BusModel` needs.
- [ ] Port `BusModel` from `pico_host.py` to MicroPython on top of `p4bridge`, keeping its semantics exactly (9-bit RX words, busy flag after every write, 4-deep TX, empty TX reads 00h after a timeout). Use temporary stand-ins for `rp2` and `tsbus`, with `pico_host.micropython_env()` as the checklist.
- [ ] Install `src/TS`, `config.ini`, `words.txt` and the assets; start `TS.tspico.TS2068_IO()` on its thread.
- [ ] Write a C test driver that replays the Phase 0 traces through `bridge_frame()` and compares the replies.
- [ ] **Firmware upstream:** draft the platform-layer change (§1.3) from what the prototype actually needed. Open it as a firmware PR.

**Gate 1:** the traces replay with matching replies on the P4, using the microSD card. If this fails: put the firmware on a separate RP2040/RP2350 attached by UART or SPI. That's real hardware, which the TS-Pico team already builds.

### Phase 2: Choose the Z80 core and lift the machine (Q2, Q6)
- [ ] Lift Fuse's TS2068 machine, SCLD and paging behind the `machine_*` API. Compile it for the host first, then the P4. Record every extra file it pulls in (Q6).
- [ ] Fix DOCK/EXROM contention for the TS2068 in the lifted module (§3, "Contention"). Add the DOCK-vs-HOME timing program to `/test`.
- [ ] Wrap two cores behind the same Z80 interface:
  - Fuse's core: generate its opcode files on the host.
  - ESPectrum's Z80_JLS: strip its ESPectrum-specific hooks.
- [ ] Host check: boot the TS2068 ROM on both cores and compare against fuse-tspico (screen and memory after N frames).
- [ ] Accuracy: run a Z80 instruction-exerciser suite on both cores (zexdoc/zexall, or Fuse's `coretest`).
- [ ] P4 benchmark: a fixed BASIC loop, frame timing with no display. Keep Z80-visible RAM and hot tables in L2MEM.
- [ ] Record the decision, with the numbers, in `docs/DECISIONS.md`.

**Gate 2:** at least one core holds a sustained emulated frame under 16.6 ms (one 60.1145 Hz frame) with headroom for display and audio, and passes the exerciser. A tie goes to Fuse's core (GPL-2+, same code base as the machine).

### Phase 3: P4 platform layer
- [ ] **Build system:** ESP-IDF components (CMake) for each layer. No autotools.
- [ ] **Display:** 1024×600 panel. The TS2068's 320×240 picture (screen plus border) doubled is 640×480, which leaves a 384-pixel side panel. Handle hi-res and decide whether to follow the beam (Q7); TSRun's raster model is the reference description.
- [ ] **Audio:** I2S to the onboard amplifier, carrying the SCLD beeper and the AY. Pace the frame loop by the audio, as ESPectrum and TSRun both do, and emit edges at the T-state they happen.
- [ ] **Storage:** microSD for ROMs, settings and snapshots.
- [ ] **Input, stage 1:** an on-screen TS2068 keyboard with multi-touch, so CAPS SHIFT and SYMBOL SHIFT combinations work.
- [ ] **Input, stage 2:** a physical keyboard, decided by Q3/Q4. Options: USB host, BLE via the C6, or a real matrix keyboard on an I2C GPIO expander.
- [ ] **ROMs:** split `TSPICO-21.ROM` into 16K HOME and 16K EXROM.
- [ ] **OSD:** a side-panel menu and file browser, following ESPectrum's OSD as the model.

**Gate 3:** the TS2068 boots to BASIC with display, sound and keyboard, and a test program runs at full speed.

### Phase 4: Connect the TS-Pico
- [ ] Port Fuse's `tspico.c` hook into `/tspico`, calling `p4bridge` in place of sockets.
- [ ] Map the firmware's SD view to the microSD card (Q5), using the same layout as a real TS-Pico card. Give the Pico flash view (`config.ini`, `assets`, `TMP`) its own folder or a flash partition.
- [ ] Switch from the temporary stand-ins to the upstream platform layer (§1.3), once that has merged.
- [ ] Run the Phase 0 sessions on the P4 and compare them with the desktop traces.

**Gate 4:** the same sessions behave the same on the P4 and on fuse-tspico + `pico_host`.

### Phase 5: Finish
- [ ] Firmware updates by copying a new `src/TS` onto the microSD card, with no reflash.
- [ ] Release as GPL-3, or GPL-2+ if the Fuse core was chosen.
- [ ] Upstream: the platform layer and in-process transport into tspico-firmware-build; bug fixes back to Fuse and fuse-tspico.

---

## 5. Working rules for Claude Code sessions

- **The firmware is the same code on every platform.** No P4-only forks of `src/TS`. A change the P4 needs goes into tspico-firmware-build as a normal upstream change that works on the RP2040 board and under `pico_host` too.
- **Bridge semantics are a contract.** Change `EMULATOR_BRIDGE.md` first, bump its version, and keep fuse-tspico and zesarux-tspico working.
- **Two references, split by subject.** For TS-Pico and bridge behavior, the reference is fuse-tspico + `pico_host` with `TSPICO_TRACE=1`. For TS2068 machine behavior (SCLD timing, contention, paging), the reference is Timex documentation and measurements on a real TS2068. Fuse and ZEsarUX are not references for those (§1.6).
- **Lifted code keeps its license headers and attribution.** Track where each file came from (repo, commit, path) in `third_party/PROVENANCE.md`.
- **No TSRun code.** Its README can be read as documentation, nothing more, until it has a license.
- **Record decisions in `docs/DECISIONS.md`:** what was decided, why, and what was ruled out.
- **Each row in §3 stays unverified until settled.** Then move it to §2, with its source or test.
- **Don't guess ESP-IDF, MicroPython or board details.** Read the pinned source tree or the Elecrow wiki.

---

## 6. Related, out of scope: a browser TS-Pico

TSRun with the real firmware on MicroPython's WebAssembly port would put a TS2068 with a working TS-Pico one link away on timexsinclair.com. `tspico-firmware-build` already lists TSRun as the next bridge target (issue #35). It depends on a license and upstream agreement from TSRun's author. Technically, TSRun's `ioRead` is synchronous, so the emulator likely needs to run in a Worker blocking on a SharedArrayBuffer, and those require cross-origin isolation headers, which GitHub Pages can't set without a workaround. The §1.3 platform layer would serve this too, as a `wasm` backend.

---

## 7. References

- Board: [Elecrow wiki: CrowPanel Advanced 7" ESP32-P4](https://www.elecrow.com/wiki/CrowPanel_Advanced_7inch_ESP32-P4_HMI_AI_Display_1024x600_IPS_Touch_Screen_with_WiFi6_Compatible_with_ArduinoLVGL.html) · [CNX Software review](https://www.cnx-software.com/2025/10/26/crowpanel-advance-7-inch-esp32-p4-hmi-display-supports-zigbee-lora-and-2-4ghz-radio-modules/) · [OpenELAB listing](https://openelab.io/a/s/products/crowpanel-advanced-7inch-esp32-p4-hmi-ai-display)
- Emulators: [fuse-tspico](https://github.com/timex-sinclair-projects/fuse-tspico) · [fuse-for-macos-tspico](https://github.com/timex-sinclair-projects/fuse-for-macos-tspico) · [zesarux-tspico](https://github.com/timex-sinclair-projects/zesarux-tspico) · [ZX-ESPectrum-IDF](https://github.com/EremusOne/ZX-ESPectrum-IDF) · [TSRun](https://github.com/josef-jelinek/TSRun) (reference only)
- Firmware: [tspico-firmware-build](https://github.com/timex-sinclair-projects/tspico-firmware-build) (`docs/EMULATOR_BRIDGE.md`, `docs/PROTOCOL.md`, `tools/emu/pico_host.py`, `src/TS`, `src/tsbus`)
- MicroPython: [ESP32_GENERIC_P4](https://micropython.org/download/ESP32_GENERIC_P4/) · [v1.27 coverage](https://www.cnx-software.com/2025/12/16/micropython-v1-27-adds-support-for-esp32-c5-esp32-p4-and-stm32u5-microcontrollers/)
