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

Present: git, gh, cmake, python3 (pyenv), perl, autoconf, automake, pkg-config. Missing: ESP-IDF and ninja (ESP-IDF's installer provides its own).
