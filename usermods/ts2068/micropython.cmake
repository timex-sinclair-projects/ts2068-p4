# ts2068: the emulated TS2068 (machine/ on the z80/ core) as a MicroPython
# user C module. For now it only runs the Phase 2 benchmark (modts2068.c).
set(TS2068_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

add_library(usermod_ts2068 INTERFACE)

target_sources(usermod_ts2068 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modts2068.c
    ${TS2068_ROOT}/machine/ts2068.c
    ${TS2068_ROOT}/machine/event.c
    ${TS2068_ROOT}/z80/z80.c
    ${TS2068_ROOT}/z80/z80_ops.c
    ${TS2068_ROOT}/test/machine/basic_loop.c
)

target_include_directories(usermod_ts2068 INTERFACE
    ${TS2068_ROOT}/compat
    ${TS2068_ROOT}/z80
    ${TS2068_ROOT}/machine
    ${TS2068_ROOT}/test/machine
    ${TS2068_ROOT}              # z80_ops.c includes "z80/opcodes_base.c"
)

target_compile_definitions(usermod_ts2068 INTERFACE HAVE_ENOUGH_MEMORY)

target_link_libraries(usermod INTERFACE usermod_ts2068)
