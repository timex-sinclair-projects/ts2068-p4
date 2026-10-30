# p4bridge: the TS-Pico bus model as a MicroPython user C module (docs/DECISIONS.md D4).
add_library(usermod_p4bridge INTERFACE)

target_sources(usermod_p4bridge INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/p4bridge.c
    ${CMAKE_CURRENT_LIST_DIR}/replay.c
)

target_include_directories(usermod_p4bridge INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_p4bridge)
