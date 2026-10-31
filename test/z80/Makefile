# Z80 core tests, on either core (z80/core.mk: CORE=fuse, the default, or jls).
#   make -C test/z80 check     # Fuse's coretest: tests.in against tests.expected
#   make -C test/z80 zex       # zexdoc and zexall (a minute or so)
ROOT := ../..
CFLAGS ?= -O2 -Wall
JLS_CPPFLAGS := -DCORETEST
include $(ROOT)/z80/core.mk
CPPFLAGS := -DCORETEST $(CORE_CPPFLAGS)

coretest$(SUFFIX): coretest.c $(CORE_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ coretest.c $(CORE_SRC)

zexrun$(SUFFIX): zexrun.c $(CORE_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ zexrun.c $(CORE_SRC)

# Frank Cringle's exercisers (zex/README.md)
zex: zexrun$(SUFFIX)
	./zexrun$(SUFFIX) zex/zexdoc.com
	./zexrun$(SUFFIX) zex/zexall.com

check: coretest$(SUFFIX)
	./coretest$(SUFFIX) tests.in > tests$(SUFFIX).out
	diff -u tests.expected tests$(SUFFIX).out && echo "coretest ($(CORE)): PASS"

clean:
	rm -f coretest coretest-jls zexrun zexrun-jls tests.out tests-jls.out jls-*.o

.PHONY: check zex clean
