# The TS2068 machine on the host.
#   make -C test/machine            # builds run_ts2068 (CORE=jls: run_ts2068-jls)
#   make -C test/machine compare    # every ROM against fuse-tspico, then the
#                                   # timing ROM with the TS2068's contention
#   make -C test/machine sound      # every sound sample against fuse-tspico,
#                                   # exactly; then the P4's float filters
#                                   # (run_ts2068-float), to within 1
#   make -C test/machine szx        # SZX snapshots against libspectrum and
#                                   # fuse-tspico (needs ~/esp/ref/prefix)
#   make -C test/machine q10probe   # the SCLD contention probe's counts here
#                                   # (PLAN.md Q10); q10probe.tap for a TS2068
# compare needs ~/esp/ref/fuse-tspico (docs/NOTES.md) and sjasmplus.
ROOT := ../..
CFLAGS ?= -O2 -Wall
JLS_CPPFLAGS := -I$(ROOT)/machine
include $(ROOT)/z80/core.mk
CPPFLAGS := $(CORE_CPPFLAGS) -I$(ROOT)/machine
SOUND := $(addprefix $(ROOT)/machine/sound/,sound.c ay_engine.c blipbuffer.c tv_filter.c ula_filter.c)
SRC := run_ts2068.c basic_loop.c $(ROOT)/machine/ts2068.c $(ROOT)/machine/event.c \
       $(ROOT)/machine/szx.c $(SOUND)
LIBS := -lm -lz

run_ts2068$(SUFFIX): $(SRC) $(CORE_DEPS) $(wildcard $(ROOT)/machine/*.h $(ROOT)/machine/sound/*.h basic_loop.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(CORE_SRC) $(LIBS)

TIMING := $(ROOT)/test/basic
$(TIMING)/dockhome-home.rom $(TIMING)/dockhome-exrom.rom: $(TIMING)/dockhome.asm
	cd $(TIMING) && sjasmplus --nologo dockhome.asm

timing-rom: $(TIMING)/dockhome-home.rom

$(TIMING)/beam-home.rom $(TIMING)/beam-exrom.rom: $(TIMING)/beam.asm
	cd $(TIMING) && sjasmplus --nologo beam.asm

beam-rom: $(TIMING)/beam-home.rom

$(TIMING)/sound-home.rom $(TIMING)/sound-exrom.rom: $(TIMING)/sound.asm
	cd $(TIMING) && sjasmplus --nologo sound.asm

# The contention probe: the code, and a TAP with its BASIC loader
$(TIMING)/q10probe.bin $(TIMING)/q10probe-code.tap: $(TIMING)/q10probe.asm
	cd $(TIMING) && rm -f q10probe-code.tap && sjasmplus --nologo q10probe.asm

$(TIMING)/q10probe.tap: $(TIMING)/q10loader.bas $(TIMING)/q10probe-code.tap
	cd $(TIMING) && zmakebas -a 10 -n q10probe -o q10loader.tap q10loader.bas && \
	    cat q10loader.tap q10probe-code.tap > q10probe.tap && rm q10loader.tap

q10probe: run_ts2068 $(TIMING)/q10probe.bin $(TIMING)/q10probe.tap
	python3 $(ROOT)/tools/host/q10probe.py

# The P4's sound arithmetic (machine/sound/sound_real.h)
run_ts2068-float$(SUFFIX): $(SRC) $(CORE_DEPS) $(wildcard $(ROOT)/machine/*.h $(ROOT)/machine/sound/*.h basic_loop.h)
	$(CC) $(CPPFLAGS) -DSOUND_REAL=float $(CFLAGS) -o $@ $(SRC) $(CORE_SRC) $(LIBS)

# libspectrum's SZX reader and writer, the reference build (docs/NOTES.md)
LIBSPECTRUM := $(HOME)/esp/ref/prefix
szx_resave: $(ROOT)/tools/host/szx_resave.c
	$(CC) $(CFLAGS) -I$(LIBSPECTRUM)/include -o $@ $< -L$(LIBSPECTRUM)/lib -lspectrum \
	    -Wl,-rpath,$(LIBSPECTRUM)/lib

szx: run_ts2068 run_ts2068-float szx_resave $(TIMING)/sound-home.rom
	python3 $(ROOT)/tools/host/compare_szx.py

sound: run_ts2068 run_ts2068-float $(TIMING)/sound-home.rom
	python3 $(ROOT)/tools/host/compare_sound.py
	python3 $(ROOT)/tools/host/compare_sound.py --run run_ts2068-float --tolerance 1

compare: run_ts2068$(SUFFIX) timing-rom beam-rom
	python3 $(ROOT)/tools/host/compare_fuse.py --core $(CORE)
	python3 $(ROOT)/tools/host/compare_fuse.py --core $(CORE) --rom timing --frames 30 --ts2068

clean:
	rm -rf run_ts2068 run_ts2068-jls run_ts2068-float szx_resave jls-*.o out

.PHONY: compare timing-rom beam-rom sound szx clean
