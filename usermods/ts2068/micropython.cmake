# ts2068: the emulated TS2068 (machine/ on a Z80 core) as a MicroPython
# user C module: the Phase 2 benchmark, and the machine in real time on the
# panel (platform/p4_display.c) with sound (machine/sound/, platform/p4_audio.c)
# and the on-screen keyboard and menu (platform/p4_touch.c, p4_keyboard.c,
# p4_osd.c).
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
    ${TS2068_ROOT}/machine/szx.c
    ${TS2068_ROOT}/machine/sound/sound.c
    ${TS2068_ROOT}/machine/sound/ay_engine.c
    ${TS2068_ROOT}/machine/sound/blipbuffer.c
    ${TS2068_ROOT}/machine/sound/tv_filter.c
    ${TS2068_ROOT}/machine/sound/ula_filter.c
    ${TS2068_CORE_SRC}
    ${TS2068_ROOT}/test/machine/basic_loop.c
    ${TS2068_ROOT}/platform/p4_display.c
    ${TS2068_ROOT}/platform/p4_audio.c
    ${TS2068_ROOT}/platform/p4_touch.c
    ${TS2068_ROOT}/platform/p4_keyboard.c
    ${TS2068_ROOT}/platform/p4_osd.c
)

target_include_directories(usermod_ts2068 INTERFACE
    ${TS2068_ROOT}/compat
    ${TS2068_ROOT}/z80
    ${TS2068_ROOT}/z80_jls
    ${TS2068_ROOT}/machine
    ${TS2068_ROOT}/test/machine
    ${TS2068_ROOT}/platform
    ${TS2068_ROOT}/tspico/p4bridge
    ${TS2068_ROOT}              # z80_ops.c includes "z80/opcodes_base.c"
)

# SOUND_REAL: the sound filters in float, which the P4's FPU does, not
# Fuse's double, which it doesn't (machine/sound/sound_real.h).
# SZX_ROM_MINIZ: inflate snapshots with the miniz in the P4's ROM, not zlib
# (machine/szx.c)
target_compile_definitions(usermod_ts2068 INTERFACE HAVE_ENOUGH_MEMORY SOUND_REAL=float
    SZX_ROM_MINIZ)

# MicroPython's main component doesn't ask for these IDF components, but
# they're in the build
target_link_libraries(usermod_ts2068 INTERFACE idf::esp_lcd idf::esp_driver_ledc idf::esp_driver_ppa
    idf::esp_driver_i2s idf::esp_driver_gpio idf::esp_driver_i2c)

target_link_libraries(usermod INTERFACE usermod_ts2068)
