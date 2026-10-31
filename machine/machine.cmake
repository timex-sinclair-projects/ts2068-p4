# machine: the TS2068 behind machine.h, with its sound and SZX snapshots,
# as the CMake library ts2068_machine (sources and include paths, compiled
# into whatever links it). It brings in the Z80 core (z80/z80.cmake). The
# host tests build the same files with test/machine/Makefile.
#
# Options for the P4, as compile definitions on the target that links it:
#   SOUND_REAL=float  the sound filters in float, which the P4's FPU does,
#                     not Fuse's double (sound/sound_real.h)
#   SZX_ROM_MINIZ     inflate snapshots with the miniz in the P4's ROM, not
#                     zlib (szx.c)
if(TARGET ts2068_machine)
    return()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../z80/z80.cmake)

add_library(ts2068_machine INTERFACE)

target_sources(ts2068_machine INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ts2068.c
    ${CMAKE_CURRENT_LIST_DIR}/event.c
    ${CMAKE_CURRENT_LIST_DIR}/szx.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/sound.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/ay_engine.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/blipbuffer.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/tv_filter.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/ula_filter.c
)

target_include_directories(ts2068_machine INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(ts2068_machine INTERFACE ts2068_z80)
