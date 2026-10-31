# ts2068: the emulated TS2068 (machine/ on a Z80 core) as a MicroPython
# user C module. For now it only runs the Phase 2 benchmark (modts2068.c).
#
# The Z80 core is Fuse's (z80/) unless the environment says
# TS2068_CORE=jls at configure time, which builds Z80_JLS (z80_jls/)
# instead. Use a separate BUILD directory for each, since the choice is
# fixed when cmake configures (docs/NOTES.md, "P4 benchmark").
set(TS2068_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

if(DEFINED ENV{TS2068_CORE})
    set(TS2068_CORE $ENV{TS2068_CORE})
else()
    set(TS2068_CORE fuse)
endif()

if(TS2068_CORE STREQUAL "fuse")
    set(TS2068_CORE_SRC
        ${TS2068_ROOT}/z80/z80.c
        ${TS2068_ROOT}/z80/z80_ops.c
        ${TS2068_ROOT}/z80/z80_core_fuse.c
    )
elseif(TS2068_CORE STREQUAL "jls")
    set(TS2068_CORE_SRC ${TS2068_ROOT}/z80_jls/z80_jls.cpp)
else()
    message(FATAL_ERROR "TS2068_CORE must be fuse or jls")
endif()
message(STATUS "ts2068: Z80 core ${TS2068_CORE}")

add_library(usermod_ts2068 INTERFACE)

target_sources(usermod_ts2068 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modts2068.c
    ${TS2068_ROOT}/machine/ts2068.c
    ${TS2068_ROOT}/machine/event.c
    ${TS2068_CORE_SRC}
    ${TS2068_ROOT}/test/machine/basic_loop.c
)

target_include_directories(usermod_ts2068 INTERFACE
    ${TS2068_ROOT}/compat
    ${TS2068_ROOT}/z80
    ${TS2068_ROOT}/z80_jls
    ${TS2068_ROOT}/machine
    ${TS2068_ROOT}/test/machine
    ${TS2068_ROOT}              # z80_ops.c includes "z80/opcodes_base.c"
)

target_compile_definitions(usermod_ts2068 INTERFACE HAVE_ENOUGH_MEMORY)

target_link_libraries(usermod INTERFACE usermod_ts2068)
