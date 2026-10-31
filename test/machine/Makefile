# The TS2068 machine on the host.
#   make -C test/machine            # builds run_ts2068 (CORE=jls: run_ts2068-jls)
#   make -C test/machine compare    # every ROM against fuse-tspico, then the
#                                   # timing ROM with the TS2068's contention
#   make -C test/machine sound      # every sound sample against fuse-tspico,
#                                   # exactly; then the P4's float filters
#                                   # (run_ts2068-float), to within 1
# compare needs ~/esp/ref/fuse-tspico (docs/NOTES.md) and sjasmplus.
ROOT := ../..
CFLAGS ?= -O2 -Wall
JLS_CPPFLAGS := -I$(ROOT)/machine
include $(ROOT)/z80/core.mk
CPPFLAGS := $(CORE_CPPFLAGS) -I$(ROOT)/machine
SOUND := $(addprefix $(ROOT)/machine/sound/,sound.c ay_engine.c blipbuffer.c tv_filter.c ula_filter.c)
SRC := run_ts2068.c basic_loop.c $(ROOT)/machine/ts2068.c $(ROOT)/machine/event.c $(SOUND)

run_ts2068$(SUFFIX): $(SRC) $(CORE_DEPS) $(wildcard $(ROOT)/machine/*.h $(ROOT)/machine/sound/*.h basic_loop.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(CORE_SRC) -lm

TIMING := $(ROOT)/test/basic
$(TIMING)/dockhome-home.rom $(TIMING)/dockhome-exrom.rom: $(TIMING)/dockhome.asm
	cd $(TIMING) && sjasmplus --nologo dockhome.asm

timing-rom: $(TIMING)/dockhome-home.rom

$(TIMING)/sound-home.rom $(TIMING)/sound-exrom.rom: $(TIMING)/sound.asm
	cd $(TIMING) && sjasmplus --nologo sound.asm

# The P4's sound arithmetic (machine/sound/sound_real.h)
run_ts2068-float$(SUFFIX): $(SRC) $(CORE_DEPS) $(wildcard $(ROOT)/machine/*.h $(ROOT)/machine/sound/*.h basic_loop.h)
	$(CC) $(CPPFLAGS) -DSOUND_REAL=float $(CFLAGS) -o $@ $(SRC) $(CORE_SRC) -lm

sound: run_ts2068 run_ts2068-float $(TIMING)/sound-home.rom
	python3 $(ROOT)/tools/host/compare_sound.py
	python3 $(ROOT)/tools/host/compare_sound.py --run run_ts2068-float --tolerance 1

compare: run_ts2068$(SUFFIX) timing-rom
	python3 $(ROOT)/tools/host/compare_fuse.py --core $(CORE)
	python3 $(ROOT)/tools/host/compare_fuse.py --core $(CORE) --rom timing --frames 30 --ts2068

clean:
	rm -rf run_ts2068 run_ts2068-jls run_ts2068-float jls-*.o out

.PHONY: compare timing-rom sound clean
