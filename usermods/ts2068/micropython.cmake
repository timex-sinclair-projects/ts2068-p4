# ts2068: the emulated TS2068 as a MicroPython user C module: the Phase 2
# benchmark, and the machine in real time on the panel with sound, the
# on-screen keyboard and the menu. modts2068.c joins the layers, each of
# which has its own CMake file:
#
#   z80/z80.cmake            the Z80 core (TS2068_CORE=jls for Z80_JLS)
#   machine/machine.cmake    the TS2068, its sound and snapshots
#   platform/platform.cmake  the panel, sound, touch, keyboard and menu area
#
# and the TS-Pico's queues, tspico/p4bridge (usermods/micropython.cmake).
set(TS2068_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

include(${TS2068_ROOT}/machine/machine.cmake)
include(${TS2068_ROOT}/platform/platform.cmake)

add_library(usermod_ts2068 INTERFACE)

target_sources(usermod_ts2068 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modts2068.c
    ${TS2068_ROOT}/test/machine/basic_loop.c   # the benchmark's BASIC
)

target_include_directories(usermod_ts2068 INTERFACE
    ${TS2068_ROOT}/test/machine
    ${TS2068_ROOT}/tspico/p4bridge
)

# The machine's P4 options (machine/machine.cmake)
target_compile_definitions(usermod_ts2068 INTERFACE SOUND_REAL=float SZX_ROM_MINIZ)

target_link_libraries(usermod_ts2068 INTERFACE ts2068_machine ts2068_platform)

target_link_libraries(usermod INTERFACE usermod_ts2068)
