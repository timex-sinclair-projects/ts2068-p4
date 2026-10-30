# Decisions

What was decided, why, and what was ruled out (PLAN.md §5). Newest last.

---

## D1. Own project, borrowing modules (2026-10-25)

**Decided:** a new P4 platform layer and project structure, with the TS2068 machine and a Z80 core lifted as C modules, and the real TS-Pico firmware on MicroPython beside it.

**Ruled out:** porting Fuse, ZEsarUX or ESPectrum whole; TSRun as a base (no license); a Wi-Fi bridge to `pico_host` (development aid only); rewriting the firmware in C. Reasons in PLAN.md §1.4.

## D2. Reference sources are pinned submodules (2026-10-30)

**Decided:** `firmware` and everything in `third_party/` are git submodules pinned to a tag or commit, listed in `third_party/PROVENANCE.md`. Code we lift is copied into our own tree, so the submodules stay read-only.

**Why:** reproducible lifts, and a clear record of which upstream commit each lifted file matches when sending fixes back.

**Pins:** firmware `v2.3.1`, fuse-tspico `tspico-v1.10.0-1`, libspectrum `2d95bbd`, ZX-ESPectrum-IDF `v1.4.15`.

## D3. MicroPython and ESP-IDF versions (2026-10-30, provisional)

**Proposed:** MicroPython **v1.29.0** (latest release, 2026-08-24) with ESP-IDF **v5.5.2**, the version its `ports/esp32/README.md` recommends.

**Why not v1.27:** PLAN.md names v1.27 because that's when P4 support arrived, not as a requirement. v1.29.0 splits the P4 board into revision 3+ and `PRE_REV3` variants, which matters for this board (see below).

**Source:** `ports/esp32/README.md` at each tag:

| MicroPython | Recommended ESP-IDF | Also supported |
|---|---|---|
| v1.27.0 | v5.5.1 | v5.2, v5.2.2, v5.3, v5.4, v5.4.1, v5.4.2 |
| v1.28.0 | v5.5.1 | v5.3, v5.4, v5.4.1, v5.4.2 |
| v1.29.0 | v5.5.2 | v5.3, v5.4, v5.4.1, v5.4.2, v5.5.1, v5.5.4 |

**Board variant:** `ESP32_GENERIC_P4`, a C6 Wi-Fi variant. Which one depends on the chip revision, still to be confirmed on the board (NOTES.md, "Chip revision").

**Open until:** the stock build runs a REPL on the board (Phase 0).
