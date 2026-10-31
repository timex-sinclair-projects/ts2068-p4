# platform

The P4 platform layer, written new: `p4_display` (MIPI-DSI 1024×600), `p4_audio` (I2S), `p4_touch`, `p4_keyboard`, `p4_storage` (microSD). See PLAN.md §1.2 and Phase 3.

| File | What it is |
|---|---|
| `p4_display.c`, `p4_display.h` | The 1024×600 MIPI-DSI panel (EK79007): power, backlight, three frame buffers in PSRAM, and the TS2068's picture drawn a line at a time, only changed lines, copied by the PPA (docs/DECISIONS.md D6). Built into the MicroPython image by `usermods/ts2068` for now |
| `p4_audio.c`, `p4_audio.h` | The two NS4168 amplifiers on I2S: mono samples to both channels, a volume, and a blocking write whose wait paces the frame loop (docs/DECISIONS.md D7) |

