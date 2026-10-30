# Fuse's Z80 core tester, against the lifted core in z80/.
#   make -C test/z80 check     # runs tests.in and compares with tests.expected
ROOT := ../..
CFLAGS ?= -O2 -Wall
CPPFLAGS := -DCORETEST -DHAVE_ENOUGH_MEMORY -I$(ROOT)/compat -I$(ROOT)/z80 -I$(ROOT)
SRC := coretest.c $(ROOT)/z80/z80.c $(ROOT)/z80/z80_ops.c

coretest: $(SRC) $(wildcard $(ROOT)/z80/*.h $(ROOT)/z80/*.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC)

check: coretest
	./coretest tests.in > tests.out
	diff -u tests.expected tests.out && echo "coretest: PASS"

clean:
	rm -f coretest tests.out

.PHONY: check clean
