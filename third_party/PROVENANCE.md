# Provenance

Where every piece of third-party code in this tree came from. Lifted code keeps its license header and attribution (PLAN.md §5).

## Reference submodules (read-only)

These are pinned checkouts, used for reading and for lifting from. Nothing under them is built into the image directly.

| Path | Repo | Pin | Commit | License |
|---|---|---|---|---|
| `firmware` | https://github.com/timex-sinclair-projects/tspico-firmware-build | `v2.3.1` | `58ccbcdf4145ca07acd613c57de34639d976ef59` | MIT (code), CC BY 4.0 (docs) |
| `third_party/fuse-tspico` | https://github.com/timex-sinclair-projects/fuse-tspico | `tspico-v1.10.0-1` | `a43958dd428768b11b7d45a03197ded5ee4b84f9` | GPL-2.0-or-later |
| `third_party/libspectrum` | https://git.code.sf.net/p/fuse-emulator/libspectrum | `master` (`libspectrum-1.7.0-11`) | `2d95bbd9c70d540cf093c3dfffc6d5a0da528997` | GPL-2.0-or-later |
| `third_party/ZX-ESPectrum-IDF` | https://github.com/EremusOne/ZX-ESPectrum-IDF | `v1.4.15` | `2a2c3350eb2b26dd871fc1f98cbf4f6c93c41948` | GPL-3.0 |

`firmware` is the TS-Pico firmware itself, not a reference: `src/TS` runs on core 1 unchanged (PLAN.md §5, "The firmware is the same code on every platform").

## Lifted files

One row per file copied into `machine/`, `z80/`, `z80_jls/` or `tspico/`. Note any local change in the last column.

| Our path | Source submodule | Source path | Source commit | Local changes |
|---|---|---|---|---|
| `z80/z80.h`, `z80/z80_macros.h`, `z80/z80.c`, `z80/z80_ops.c` | `third_party/fuse-tspico` | `z80/` (same names) | `a43958d` | Fuse's hooks replaced by `z80/z80_glue.h`; see `z80/README.md` |
| `z80/opcodes_base.c`, `z80/z80_cb.c`, `z80/z80_ddfd.c`, `z80/z80_ddfdcb.c`, `z80/z80_ed.c` | `third_party/fuse-tspico` | generated: `z80/z80.pl` + `z80/opcodes_*.dat` | `a43958d` | None (generated output) |
| `test/z80/coretest.c` | `third_party/fuse-tspico` | `z80/coretest.c` | `a43958d` | Dummies for Fuse's peripherals removed; provides `z80_glue.h`'s event and /INT names |
| `test/z80/tests.in`, `test/z80/tests.expected` | `third_party/fuse-tspico` | `z80/tests/` | `a43958d` | None |
| `test/z80/zex/zexdoc.com`, `zexall.com`, `zexdoc.z80`, `zexall.z80`, `README.md`, `LICENSE` | https://github.com/agn453/ZEXALL (not a submodule; downloaded 2026-10-30) | same names | `319f7f9` | None. Frank D. Cringle's exercisers, GPL-2.0, as extracted from YAZE-AG 2.51.3 |
| `machine/ts2068.c` | `third_party/fuse-tspico` | `machines/ts2068.c`, `machines/tc2068.c`, `machines/machines_periph.c`, `peripherals/scld.c`, `peripherals/ula.c`, `peripherals/sound/ay.c`, `spectrum.c`, `machine.c`, `periph.c`, `memory_pages.c`, `display_render.c` | `a43958d` | Merged into one file and cut to the TS2068: 8K-chunk memory tables, a port switch, one contention table; DOCK and EXROM not contended. See `machine/README.md` |
| `machine/z80_machine.h` | `third_party/fuse-tspico` | `z80/z80_macros.h` (the contention macros) | `a43958d` | Keyed on an 8K-chunk flag instead of `memory_map_read[].contended` |
| `machine/event.c` | `third_party/fuse-tspico` | `event.c` (semantics only; rewritten) | `a43958d` | A fixed array, no glib |
| `machine/sound/ay_engine.c`, `ay_engine.h`, `tv_filter.c`, `tv_filter.h`, `ula_filter.c`, `ula_filter.h` | `third_party/fuse-tspico` | `sound/` (same names) | `a43958d` | Includes adjusted; the AY engine's peripheral check removed (the TS2068 always has its AY); the filters' arithmetic is `sound_real` (`double`, or `float` on the P4) |
| `machine/sound/blipbuffer.c`, `blipbuffer.h` | `third_party/fuse-tspico` | `sound/blipbuffer.c`, `blipbuffer.h` | `a43958d` | LGPL-2.1-or-later (Shay Green's Blip_Buffer 0.4.0, in C by Gergely Szasz). Accuracy 32 bits with `uint64_t` resampled time, as Fuse's 64-bit builds have it |
| `machine/sound/sound.c`, `sound.h` | `third_party/fuse-tspico` | `sound.c`, `sound/output_mixer.c` | `a43958d` | Cut to the TS2068: the TV speaker route, mono, default volumes, no main buffer, 100 ms Blip_Buffers |
| `test/basic/sound.asm` | (ours) | | | The sound test ROM |
| `machine/szx.c`, `tools/host/szx_resave.c`, `tools/host/compare_szx.py` | (ours) | | | SZX snapshots. The format is libspectrum's (`third_party/libspectrum`, `szx.c`), followed byte for byte but not copied; `szx_resave` links the reference libspectrum to check it |
| `z80_jls/z80_jls.cpp`, `z80_jls/z80_jls.h` | `third_party/ZX-ESPectrum-IDF` | `src/Z80_JLS.cpp`, `include/Z80_JLS/z80.h` | `2a2c335` | GPL-3.0. ESPectrum's hooks removed (TR-DOS, tape traps, video, CPU loop); `step`, `takeINT` and the `z80_core.h` functions added. Not linked by default. See `z80_jls/README.md` |
| `z80_jls/z80_jls_ops.h` | `third_party/ZX-ESPectrum-IDF` | `include/Z80_JLS/z80operations.h` (replaced) | `2a2c335` | Rewritten on `z80/z80_glue.h` |

## Fonts

| Our path | Source | Made by | License |
|---|---|---|---|
| `platform/p4_osd_font.h` | Aileron Regular (Sora Sagano), the font built into Pillow 12.1.1 (`ImageFont.load_default`) | `tools/host/make_osd_font.py`: ASCII 32-126 at 16 and 24 pixels, 4-bit bitmaps | CC0 1.0 |

## Facts taken from other sources

No code is copied from these. `platform/p4_display.c`, `p4_audio.c` and `p4_touch.c` use their values (register writes, timings, pins), as noted at the top of the file.

| Source | Version | License | What |
|---|---|---|---|
| Elecrow `CrowPanel-Advanced-7inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen`, `example/V1.2/idf-code/Lesson07-Turn_on_the_screen` (not a submodule; cloned to `~/esp/ref/crowpanel-p4`) | `f0ebc0c` | none stated | DSI lanes and rate, DPI timing, LDO3 at 2.5 V, backlight on GPIO31 |
| Espressif `esp_lcd_ek79007` (component registry, as vendored in that example) | 1.0.2 | Apache-2.0 | The EK79007's start-up register writes |
| Elecrow, same repo, `example/V1.2/idf-code/Lesson12-Playing_Loca_Music_from_SD_Card` | `f0ebc0c` | none stated | I2S pins (LRCLK 21, BCLK 22, DATA 23), the Philips 16-bit stereo format, the amplifier enable on GPIO30, active low (`platform/p4_audio.c`) |
| Elecrow, same repo, `example/V1.2/idf-code/Lesson05-Touchscreen` | `f0ebc0c` | none stated | The GT911's I2C pins (SDA 45, SCL 46), its address and reset/interrupt pins (GPIO40, GPIO42), and its orientation (`platform/p4_touch.c`) |

## Not used

- **TSRun** (https://github.com/josef-jelinek/TSRun): no license. Its README may be read as documentation; no code is copied.
