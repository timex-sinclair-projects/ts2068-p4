# Fuse's Z80 core tester, against the lifted core in z80/.
#   make -C test/z80 check     # runs tests.in and compares with tests.expected
#   make -C test/z80 zex       # zexdoc and zexall (minutes)
ROOT := ../..
CFLAGS ?= -O2 -Wall
CPPFLAGS := -DCORETEST -DHAVE_ENOUGH_MEMORY -I$(ROOT)/compat -I$(ROOT)/z80 -I$(ROOT)
SRC := coretest.c $(ROOT)/z80/z80.c $(ROOT)/z80/z80_ops.c

coretest: $(SRC) $(wildcard $(ROOT)/z80/*.h $(ROOT)/z80/*.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC)

zexrun: zexrun.c $(wildcard $(ROOT)/z80/*.h $(ROOT)/z80/*.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ zexrun.c $(ROOT)/z80/z80.c $(ROOT)/z80/z80_ops.c

# Frank Cringle's exercisers (zex/README.md): a few minutes each
zex: zexrun
	./zexrun zex/zexdoc.com
	./zexrun zex/zexall.com

check: coretest
	./coretest tests.in > tests.out
	diff -u tests.expected tests.out && echo "coretest: PASS"

clean:
	rm -f coretest tests.out zexrun

.PHONY: check zex clean
