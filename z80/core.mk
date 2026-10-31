# Which Z80 core a host test links: make CORE=fuse (the default) or CORE=jls.
# Include after setting ROOT. Gives CORE_SRC (C sources or objects to put on
# the link line), CORE_DEPS, CORE_CPPFLAGS and SUFFIX (for binary names), and
# the rule that builds Z80_JLS's object. Both cores implement z80_core.h.

CORE ?= fuse
CORE_CPPFLAGS := -DHAVE_ENOUGH_MEMORY -I$(ROOT)/compat -I$(ROOT)/z80 -I$(ROOT)

ifeq ($(CORE),fuse)
CORE_SRC := $(ROOT)/z80/z80.c $(ROOT)/z80/z80_ops.c $(ROOT)/z80/z80_core_fuse.c
CORE_DEPS := $(wildcard $(ROOT)/z80/*.h $(ROOT)/z80/*.c)
SUFFIX :=
else ifeq ($(CORE),jls)
# Z80_JLS is C++ without exceptions or RTTI, so it links with $(CC) and
# needs no C++ runtime.
CXXFLAGS ?= -O2 -Wall
CORE_SRC := jls-$(notdir $(CURDIR)).o
CORE_DEPS := $(CORE_SRC)
SUFFIX := -jls
$(CORE_SRC): $(wildcard $(ROOT)/z80_jls/* $(ROOT)/z80/z80_core.h $(ROOT)/z80/z80_glue.h)
	$(CXX) $(CORE_CPPFLAGS) $(JLS_CPPFLAGS) -I$(ROOT)/z80_jls $(CXXFLAGS) \
	  -std=c++17 -fno-exceptions -fno-rtti -c -o $@ $(ROOT)/z80_jls/z80_jls.cpp
else
$(error CORE must be fuse or jls)
endif
