# machine

The TS2068 behind the machine API (`machine.h`, names prefixed `ts2068_`), lifted from fuse-tspico (`third_party/fuse-tspico`, tag `tspico-v1.10.0-1`), GPL-2.0-or-later. It runs on the Z80 core in `z80/` (DECISIONS.md D5), through `z80/z80_core.h` and `z80/z80_glue.h`. It also runs unchanged on Z80_JLS (`z80_jls/`), the core that wasn't chosen. DOCK and EXROM pages are never contended here (PLAN.md §3, "Contention"), unless a config asks for Fuse's behaviour so the two can be compared.

**Checked (2026-10-30):** `make -C test/machine compare` boots three ROMs here and in fuse-tspico and compares every Z80 register, R, the T-state within the frame, and the screen, after 1 to 3,000 frames. The three ROMs are the stock TS2068 ROM, the TS-Pico ROM (`firmware/src/rom/TSPICO-23.ROM`) and the contention test (`test/basic/dockhome.asm`). All match. With the TS2068's own contention, the contention test differs from Fuse in exactly the four cases that touch DOCK or EXROM at 0x4000, and those run at the uncontended speed. On the host (Apple silicon, `-O2`) a frame takes about 80 µs. On the P4's core 0 a frame takes 2.5 ms, 15% of a 60 Hz frame (docs/NOTES.md, "P4 benchmark").

## Files

| File | What it is |
|---|---|
| `machine.h` | The API the platform sees: config and ROMs, `ts2068_run_frame`, peek and poke, keyboard and joysticks, `ts2068_render`, state for tests. PLAN.md calls it the `machine_*` API; the prefix is `ts2068_` because MicroPython's esp32 port already exports `machine_init` and many other `machine_*` names |
| `ts2068.c` | The machine. Fuse's `machines/ts2068.c` and `tc2068.c`, `peripherals/scld.c`, `ula.c` and `sound/ay.c` (the register file), `machines/machines_periph.c` (the Timex AY ports), `spectrum.c` (the frame event and contention delay), `machine.c` (timings), `periph.c` (port I/O and its contention) and `memory_pages.c` (memory access), in one file. `display_render.c`'s Timex modes for `ts2068_render` |
| `z80_machine.h` | The contention macros `z80_glue.h` asks for: Fuse's, keyed on one flag per 8K chunk |
| `event.c`, `event.h` | Fuse's event queue semantics (time, then type; a new event before an equal one) in a fixed array, without glib |
| `szx.c` | SZX snapshots, written new: the chunks libspectrum writes for a TS2068 (no code from it), saved uncompressed, loaded compressed or not (zlib on the host, the ROM's miniz on the P4). `ts2068_snap_get`/`ts2068_snap_set` in `ts2068.c` restore the state in the order Fuse's `snapshot_copy_from` does |
| `machine.cmake` | The CMake library `ts2068_machine` for the P4 build (it brings in `z80/z80.cmake`), and the P4's two options; the host tests use `test/machine/Makefile` |
| `sound/` | The sound (DECISIONS.md D7): Fuse's `sound/ay_engine.c`, `blipbuffer.c`, `tv_filter.c` and `ula_filter.c`, with includes adjusted, and `sound.c`, the part of Fuse's `sound.c` and `sound/output_mixer.c` a TS2068 uses (the TV speaker route, mono). `ts2068_sound_init` and `ts2068_sound_samples` in `machine.h` are the platform's view of it |

## How it differs from Fuse

- **Sound:** `make -C test/machine sound` runs three ROMs (the sound test `test/basic/sound.asm`, the stock ROM and the TS-Pico ROM) here and in fuse-tspico at 44.1 and 48 kHz, and every sample matches. Two things in `sound/` differ from Fuse's source. Blip_Buffer's accuracy is 32 bits on every platform, which is what Fuse's own 64-bit builds use. The filters can be built in `float` (`sound/sound_real.h`, which the P4 build does); that differs from Fuse's `double` by at most one least significant bit.

- **Memory:** Fuse's 2K `memory_page` tables, `RAM[65][0x4000]` (1,040 KB) and memory pool become three 8K-chunk tables (read pointer, write pointer or NULL, contended) over 48K of RAM, a 16K HOME ROM, a 16K EXROM and one 8K FFh chunk for the empty DOCK. The paging is Fuse's `tc2068_memory_map`.
- **Contention:** one 58,688-entry delay table (Fuse keeps two of 80,000, identical on the TS2068), filled from Fuse's `spectrum_contend_delay_65432100`. That's still the Spectrum's pattern, and whether the SCLD contends like this is unmeasured (PLAN.md §1.6, Q10). A chunk is contended only when HOME RAM5 is mapped there. `fuse_dock_contention` restores Fuse's rule, which contends DOCK and EXROM at 0x4000–0x7FFF too.
- **Ports:** every TS2068 port is decoded on its low byte and no two share one, so Fuse's peripheral list is a switch: FE (ULA), F4 and FF (SCLD), F5 and F6 (AY, with the joysticks on R14), 0E and 0F (the TS-Pico, through `machine_tspico_fn`). Anything else reads FFh, since the 2068 has no floating bus.
- **Reset:** Fuse's order. A hard reset also clears RAM; Fuse's RAM is zero only when Fuse starts.
- **Snapshots:** SZX only, and only what a stock TS2068 needs: no DOCK chunks (a snapshot with DOCK-bank pages is refused), no custom-ROM chunk. `make -C test/machine szx` checks them: libspectrum rewrites ours byte for byte, and Fuse and we run the same snapshot to the same registers, screen and sound (docs/NOTES.md, "Snapshots").

## Not yet here

- **Fuse's dirty-line tracking** (`display_dirty.c`): the picture follows the beam as Fuse's does (Q7, `test/basic/beam.asm`), but every chunk is copied and every line drawn and hashed each frame, even when nothing changed.
- **DOCK cartridges** (`dck.c`): the DOCK reads FFh.
- **RZX, the debugger, the tape and SLT traps:** not planned. The TS-Pico is the tape.
