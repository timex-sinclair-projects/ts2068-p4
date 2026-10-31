# The TS2068 machine on the host.
#   make -C test/machine            # builds run_ts2068 (CORE=jls: run_ts2068-jls)
#   make -C test/machine compare    # every ROM against fuse-tspico, then the
#                                   # timing ROM with the TS2068's contention
# compare needs ~/esp/ref/fuse-tspico (docs/NOTES.md) and sjasmplus.
ROOT := ../..
CFLAGS ?= -O2 -Wall
JLS_CPPFLAGS := -I$(ROOT)/machine
include $(ROOT)/z80/core.mk
CPPFLAGS := $(CORE_CPPFLAGS) -I$(ROOT)/machine
SRC := run_ts2068.c basic_loop.c $(ROOT)/machine/ts2068.c $(ROOT)/machine/event.c

run_ts2068$(SUFFIX): $(SRC) $(CORE_DEPS) $(wildcard $(ROOT)/machine/*.h basic_loop.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(CORE_SRC)

TIMING := $(ROOT)/test/basic
$(TIMING)/dockhome-home.rom $(TIMING)/dockhome-exrom.rom: $(TIMING)/dockhome.asm
	cd $(TIMING) && sjasmplus --nologo dockhome.asm

timing-rom: $(TIMING)/dockhome-home.rom

compare: run_ts2068$(SUFFIX) timing-rom
	python3 $(ROOT)/tools/host/compare_fuse.py --core $(CORE)
	python3 $(ROOT)/tools/host/compare_fuse.py --core $(CORE) --rom timing --frames 30 --ts2068

clean:
	rm -rf run_ts2068 run_ts2068-jls jls-*.o out

.PHONY: compare timing-rom clean
