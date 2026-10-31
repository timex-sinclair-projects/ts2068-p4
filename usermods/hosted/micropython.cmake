# hosted: the ESP32-C6 co-processor's Bluetooth controller, turned on
# through esp-hosted before MicroPython's bluetooth module starts NimBLE.
add_library(usermod_hosted INTERFACE)

target_sources(usermod_hosted INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modhosted.c
)

target_link_libraries(usermod INTERFACE usermod_hosted)
