# test

- `traces/`: reference bridge traces recorded from fuse-tspico + `pico_host` with `TSPICO_TRACE=1` (Phase 0).
- `z80/`: Z80 instruction-exerciser suites. `make -C test/z80 check` runs Fuse's coretest; `make -C test/z80 zex` runs zexdoc and zexall (`zex/`, from agn453/ZEXALL) through `zexrun`, a minimal CP/M host. Add `CORE=jls` to either target, or to `make -C test/machine compare`, to run on Z80_JLS instead of Fuse's core.
- `basic/`: BASIC and machine-code test programs. `dockhome.asm` is the DOCK-vs-HOME contention timing program: a test ROM pair (`make -C test/machine timing-rom`, needs `sjasmplus`).
- `machine/`: `run_ts2068`, the machine on the host. `make -C test/machine compare` boots the stock, TS-Pico and timing ROMs against fuse-tspico (`tools/host/compare_fuse.py`).
