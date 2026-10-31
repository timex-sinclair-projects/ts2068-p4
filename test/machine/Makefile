# The TS2068 machine on the host.
#   make -C test/machine            # builds run_ts2068
#   make -C test/machine compare    # every ROM against fuse-tspico, then the
#                                   # timing ROM with the TS2068's contention
# compare needs ~/esp/ref/fuse-tspico (docs/NOTES.md) and sjasmplus.
ROOT := ../..
CFLAGS ?= -O2 -Wall
CPPFLAGS := -DHAVE_ENOUGH_MEMORY -I$(ROOT)/compat -I$(ROOT)/z80 -I$(ROOT)/machine -I$(ROOT)
SRC := run_ts2068.c basic_loop.c $(ROOT)/machine/ts2068.c $(ROOT)/machine/event.c \
       $(ROOT)/z80/z80.c $(ROOT)/z80/z80_ops.c

run_ts2068: $(SRC) $(wildcard $(ROOT)/z80/*.h $(ROOT)/z80/*.c $(ROOT)/machine/*.h basic_loop.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC)

TIMING := $(ROOT)/test/basic
$(TIMING)/dockhome-home.rom $(TIMING)/dockhome-exrom.rom: $(TIMING)/dockhome.asm
	cd $(TIMING) && sjasmplus --nologo dockhome.asm

timing-rom: $(TIMING)/dockhome-home.rom

compare: run_ts2068 timing-rom
	python3 $(ROOT)/tools/host/compare_fuse.py
	python3 $(ROOT)/tools/host/compare_fuse.py --rom timing --frames 30 --ts2068

clean:
	rm -rf run_ts2068 out

.PHONY: compare timing-rom clean
