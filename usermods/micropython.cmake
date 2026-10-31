# Our MicroPython user C modules. Pass this file's absolute path as
# USER_C_MODULES when building MicroPython's esp32 port (see docs/NOTES.md).
include(${CMAKE_CURRENT_LIST_DIR}/../tspico/p4bridge/micropython.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ts2068/micropython.cmake)
