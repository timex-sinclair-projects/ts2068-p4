# z80: the Z80 core behind z80_core.h, as the CMake library ts2068_z80
# (sources and include paths, compiled into whatever links it). Include it
# from an ESP-IDF or MicroPython build; the host tests use core.mk.
#
# The core is Fuse's (this folder) unless TS2068_CORE, set before including
# this file or in the environment at configure time, says jls, which builds
# Z80_JLS (z80_jls/) instead. The choice is fixed when cmake configures, so
# use a separate build directory for each (docs/NOTES.md, "P4 benchmark").
if(TARGET ts2068_z80)
    return()
endif()

set(TS2068_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

if(NOT DEFINED TS2068_CORE)
    if(DEFINED ENV{TS2068_CORE})
        set(TS2068_CORE $ENV{TS2068_CORE})
    else()
        set(TS2068_CORE fuse)
    endif()
endif()
message(STATUS "ts2068: Z80 core ${TS2068_CORE}")

add_library(ts2068_z80 INTERFACE)

if(TS2068_CORE STREQUAL "fuse")
    target_sources(ts2068_z80 INTERFACE
        ${TS2068_ROOT}/z80/z80.c
        ${TS2068_ROOT}/z80/z80_ops.c
        ${TS2068_ROOT}/z80/z80_core_fuse.c
    )
elseif(TS2068_CORE STREQUAL "jls")
    target_sources(ts2068_z80 INTERFACE ${TS2068_ROOT}/z80_jls/z80_jls.cpp)
    target_include_directories(ts2068_z80 INTERFACE ${TS2068_ROOT}/z80_jls)
else()
    message(FATAL_ERROR "TS2068_CORE must be fuse or jls")
endif()

target_include_directories(ts2068_z80 INTERFACE
    ${TS2068_ROOT}/compat
    ${TS2068_ROOT}/z80
    ${TS2068_ROOT}              # z80_ops.c includes "z80/opcodes_base.c"
)

target_compile_definitions(ts2068_z80 INTERFACE HAVE_ENOUGH_MEMORY)
