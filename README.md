# esp32-2068

A standalone Timex/Sinclair 2068 with a TS-Pico, running on an Elecrow CrowPanel Advanced 7" ESP32-P4.

The emulator runs on one P4 core; the real TS-Pico MicroPython firmware runs on the other. See [PLAN.md](PLAN.md) for the architecture and phases.

## Layout

| Path | Contents |
|---|---|
| `main/` | `app_main`, task and core setup |
| `boards/` | `CROWPANEL_P4_7`, the MicroPython board this is built as (a 4 MB app partition) |
| `platform/` | P4 display, audio, touch, keyboard, storage |
| `machine/` | TS2068 machine, SCLD and paging (lifted from Fuse) |
| `z80/` | Z80 core (chosen in Phase 2) |
| `tspico/` | Bridge hook and `p4bridge` queues |
| `firmware/` | Submodule: tspico-firmware-build (pinned) |
| `third_party/` | Read-only reference submodules; see [PROVENANCE.md](third_party/PROVENANCE.md) |
| `docs/` | [DECISIONS.md](docs/DECISIONS.md), [NOTES.md](docs/NOTES.md) |
| `test/` | Trace fixtures, Z80 suites, BASIC test programs |

## Getting the source

```bash
git clone --recurse-submodules https://github.com/timex-sinclair-projects/ts2068-p4.git
```

## Running it

Build MicroPython with our board and modules, flash it, then put our files on the board's flash and the ROMs on its microSD card (docs/NOTES.md has the details):

```bash
cd ~/esp/micropython/ports/esp32
make BOARD_DIR=<repo>/boards/CROWPANEL_P4_7 USER_C_MODULES=<repo>/usermods/micropython.cmake
esptool --chip esp32p4 write-flash 0x10000 build-CROWPANEL_P4_7/micropython.bin
cd <repo>
python3 tools/p4/install.py
python3 tools/p4/sd_roms.py
```

At power-on `main.py` starts the TS2068 from the card's `/TS2068` folder, with the TS-Pico firmware plugged in when the ROM is the 32K TS-Pico one, and puts the menu in the side panel above the on-screen keyboard. To boot to the REPL instead, create `/p4/noboot` on the flash.

## License

To be set in Phase 2: GPL-2.0-or-later if Fuse's Z80 core is chosen, GPL-3.0 if Z80_JLS is (PLAN.md §2, "Licensing outcome").
