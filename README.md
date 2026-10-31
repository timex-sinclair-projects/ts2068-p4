# esp32-2068

A standalone Timex/Sinclair 2068 with a TS-Pico, running on an Elecrow CrowPanel Advanced 7" ESP32-P4.

The emulator runs on one P4 core; the real TS-Pico MicroPython firmware runs on the other. See [PLAN.md](PLAN.md) for the architecture and phases.

## Layout

| Path | Contents |
|---|---|
| `boards/` | `CROWPANEL_P4_7`, the MicroPython board this is built as (a 4 MB app partition) |
| `usermods/` | The MicroPython modules that join the layers: `ts2068` (the machine on the panel, plus `emu.py`, `osd.py`, `btkbd.py`, `kbdmap.py`, `main.py`), `p4bridge`, and `hosted` (the C6's Bluetooth controller). `usermods/micropython.cmake` is the build's entry point |
| `platform/` | P4 display, audio, touch, on-screen keyboard, menu area (`platform.cmake`) |
| `machine/` | TS2068 machine, SCLD, paging and sound, lifted from Fuse (`machine.cmake`) |
| `z80/` | Fuse's Z80 core (`z80.cmake`); `z80_jls/` is the one not chosen |
| `tspico/` | `p4bridge` queues, and the Python that runs the firmware on the P4 |
| `tools/` | `p4/`: install, card setup, sessions; `host/`: comparisons against Fuse |
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

At power-on `main.py` starts the TS2068 from the card's `/TS2068` folder, with the TS-Pico firmware plugged in when the ROM is the 32K TS-Pico one, and puts the menu in the side panel above the on-screen keyboard (or in the whole side panel while a Bluetooth keyboard is connected). To boot to the REPL instead, create `/p4/noboot` on the flash.

It needs MicroPython v1.29.0 and ESP-IDF v5.5.2 (docs/DECISIONS.md D3).

**A Bluetooth keyboard:** tap **Keyboard → Pair a keyboard** in the menu with the keyboard in pairing mode. It's remembered, and reconnects at power-on when a key is pressed. The keys type what they say; Ctrl or Alt is SYMBOL SHIFT, Esc is BREAK, Tab is extended mode. BLE keyboards only: the C6 has no Bluetooth Classic. Pairing has no passkey for now (docs/DECISIONS.md D8).

## License

GPL-2.0-or-later (`LICENSE`), since Fuse's Z80 core was chosen (docs/DECISIONS.md D5). Lifted code keeps its own header and attribution (`third_party/PROVENANCE.md`):

- `machine/`, `z80/`: from Fuse and fuse-tspico, GPL-2.0-or-later.
- `z80_jls/`: ESPectrum's Z80_JLS, **GPL-3.0**. Nothing links it by default; a build made with it (`TS2068_CORE=jls`) is GPL-3.0 as a whole.
- The TS-Pico firmware (`firmware`, a submodule) and MicroPython: MIT.
- `platform/p4_osd_font.h`: Aileron Regular, CC0 1.0.
