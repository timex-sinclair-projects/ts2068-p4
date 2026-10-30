# Notes

Working notes: findings, measurements and things to check. Settled facts move to PLAN.md §2 with their source.

---

## Chip revision (2026-10-30, unverified)

MicroPython's `ESP32_GENERIC_P4/board.md` (v1.29.0) says the P4 changed substantially at chip revision 3.0, and that Espressif marks revision 3+ parts with an **X** suffix (`ESP32-P4NRW32X`). PLAN.md §2 lists this board's SoC as `ESP32-P4NRW32`, with no X, which would make it a revision 0.x/1.x chip that needs the `PRE_REV3_C6_WIFI` variant.

The part number in PLAN.md came from listings and reviews, so it may not show the suffix. **Check on the board:** `esptool.py --chip esp32p4 chip_id` (or the boot log's `chip revision: vX.Y`). Then choose `C6_WIFI` (rev 3+) or `PRE_REV3_C6_WIFI` (rev < 3), and set `CONFIG_ESP32P4_REV_MIN` to match in our own sdkconfig.

## Local checkouts that already exist (2026-10-30)

In `~/Documents/github`:

- `ESP2068`: an earlier port of ESPectrum to the TS2068 (factus10/ESP2068, last commit 2026-07-11, "Phase 4 Piece D: confirm the ROM patches against a real, live Z80 core"). Not mentioned in PLAN.md. It may hold useful TS2068 ROM patch work and Z80_JLS findings, which are relevant to Phase 2.
- `fuse-tspico-lab`: a fuse-tspico working tree on `tspico-device` at `a3d27c57` (2026-10-06), two days newer than the `tspico-v1.10.0-1` tag pinned here. It already has a `build-null` build and split `tspico-home.rom` / `tspico-exrom.rom`. That's a head start on Phase 0's reference build.
- `zesarux-tspico-lab`: zesarux-tspico working tree.
- `tspico-firmware-build`: on branch `megabank-bus-sharing` at `fd9db5c`, ahead of the `v2.3.1` pinned here.

## Toolchain on this Mac (2026-10-30)

Present: git, gh, cmake, python3 (pyenv), perl, autoconf, automake, pkg-config. Missing: ESP-IDF and ninja (ESP-IDF's installer provides its own).
