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
