# platform: the CrowPanel's panel, sound, touch, on-screen keyboard and
# menu area, as the CMake library ts2068_platform (sources and include
# paths, compiled into whatever links it). It needs nothing from the
# machine; it asks for the IDF components it uses.
if(TARGET ts2068_platform)
    return()
endif()

add_library(ts2068_platform INTERFACE)

target_sources(ts2068_platform INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/p4_display.c
    ${CMAKE_CURRENT_LIST_DIR}/p4_audio.c
    ${CMAKE_CURRENT_LIST_DIR}/p4_touch.c
    ${CMAKE_CURRENT_LIST_DIR}/p4_keyboard.c
    ${CMAKE_CURRENT_LIST_DIR}/p4_osd.c
)

target_include_directories(ts2068_platform INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(ts2068_platform INTERFACE idf::esp_lcd idf::esp_driver_ledc
    idf::esp_driver_ppa idf::esp_driver_i2s idf::esp_driver_gpio idf::esp_driver_i2c)
