# test

- `traces/`: reference bridge traces recorded from fuse-tspico + `pico_host` with `TSPICO_TRACE=1` (Phase 0).
- `z80/`: Z80 instruction-exerciser suites (zexdoc/zexall, Fuse coretest).
- `basic/`: BASIC and machine-code test programs. `dockhome.asm` is the DOCK-vs-HOME contention timing program: a test ROM pair (`make -C test/machine timing-rom`, needs `sjasmplus`).
- `machine/`: `run_ts2068`, the machine on the host. `make -C test/machine compare` boots the stock, TS-Pico and timing ROMs against fuse-tspico (`tools/host/compare_fuse.py`).
