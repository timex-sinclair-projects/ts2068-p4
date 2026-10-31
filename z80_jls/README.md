# z80_jls

ESPectrum's Z80_JLS, lifted from `third_party/ZX-ESPectrum-IDF` (tag `v1.4.15`, `2a2c335`), GPL-3.0. It's José Luis Sánchez's Z80Core, converted to C++, with ESPectrum's optimizations. It was the other Phase 2 candidate. **Fuse's core (`z80/`) was chosen** (docs/DECISIONS.md, D5), so nothing links this by default. It stays here, behind the same interface, so the comparison can be rerun.

It's GPL-3.0, unlike the rest of the project. A build that links it is GPL-3 overall (PLAN.md §1.5).

**Checked (2026-10-30):** zexdoc and zexall pass 67 of 67 each. The TS2068 machine on this core matches fuse-tspico (every register, R, the T-state within the frame, and the screen) after 1 to 3,000 frames, for all three ROMs `make -C test/machine compare` runs. Fuse's `coretest` passes 1,345 of 1,356 tests; DECISIONS.md D5 explains the 11 that differ. On the P4 a frame takes 2.9 ms (Fuse's core: 2.5 ms).

## Building with it

| Where | How |
|---|---|
| Host tests | `make -C test/z80 check CORE=jls`, `make -C test/z80 zex CORE=jls`, `make -C test/machine compare CORE=jls`. Binaries get a `-jls` suffix |
| P4 | `TS2068_CORE=jls` in the environment when cmake configures the MicroPython build, with its own `BUILD` directory (docs/NOTES.md, "P4 benchmark") |

## Files

| File | From | Changes |
|---|---|---|
| `z80_jls.h` | `include/Z80_JLS/z80.h` | Includes `z80_jls_ops.h`. `execute`, `exec_nocheck` and `exec_nocheck_2A3` become `step`; adds `takeINT`; `check_trdos` removed |
| `z80_jls_ops.h` | `include/Z80_JLS/z80operations.h` (replaced) | `Z80Ops` on `z80/z80_glue.h`: `readbyte`, `writebyte`, `readport`, `writeport` and the contention macros, the same calls Fuse's core makes. ESPectrum's per-machine function pointers are gone. `VIDEO::Draw(n)` becomes `Z80Ops::internal(n)`, which adds T-states without contention |
| `z80_jls.cpp` | `src/Z80_JLS.cpp` | ESPectrum's includes removed. Removed too: TR-DOS paging (`check_trdos` and its 30 calls), the tape LOAD trap (CP A at 056Bh) and SAVE trap (DEC IX at 04D4h), and HALT's `CPU::stFrame` signal. `Ports::input`/`output` become `Z80Ops::input`/`output`. The instructions are otherwise unchanged. Added at the end: `Z80::step`, `Z80::takeINT`, and the `z80_core.h` functions |

The lift was done by script, so the unchanged parts match upstream line for line, ignoring the deleted blocks. ESPectrum's commented-out code is left in place.

## How it fits `z80_core.h`

- **`step()`** runs one whole instruction, with its DD, ED or FD prefix. It fetches through `contend_read(PC, 4)` and `readbyte_internal`, like Fuse's core, then checks /INT while `tstates < z80_interrupt_length`. `z80_do_opcodes` calls it until `tstates` reaches `event_next_event`.
- **HALT:** Z80_JLS leaves PC after the HALT, and Fuse leaves it on the HALT. `z80_get_regs` and `z80_set_regs` convert, so the tests see Fuse's convention. While halted, `step` fetches from the HALT's address, as Fuse does, so contention matches.
- **Reset:** `z80_reset` gives Fuse's values (AF, AF' and SP are FFFFh, and a hard reset zeroes the other pairs), not Z80_JLS's (every pair FFFFh). That way runs on the two cores start the same.
- **Interrupts:** `z80_interrupt` takes the interrupt if Z80_JLS's `checkINT` would: IFF1 set, not straight after EI, and /INT low. `z80_frame` does nothing, since Z80_JLS keeps no times of its own.
