# CROWPANEL_P4_7: MicroPython for the CrowPanel Advanced 7" ESP32-P4, as
# MicroPython's ESP32_GENERIC_P4 with the PRE_REV3_C6_WIFI variant (a v1.3
# chip, Wi-Fi through the C6: docs/NOTES.md), but a 4 MB app partition
# (partitions.csv) instead of 1.94 MB, and sdkconfig.board's settings for
# the panel. MicroPython makes the filesystem from the rest of the flash.
#
#   make BOARD_DIR=<this directory> USER_C_MODULES=<repo>/usermods/micropython.cmake
#
# The generic board's files are included from the port, so the paths are
# relative to it, as they are there.
include(boards/ESP32_GENERIC_P4/mpconfigboard.cmake)
include(boards/ESP32_GENERIC_P4/mpconfigvariant_PRE_REV3_C6_WIFI.cmake)

# sdkconfig can't name a file outside the port except by absolute path, so
# the path to partitions.csv is written at configure time
set(CROWPANEL_PARTITIONS ${CMAKE_BINARY_DIR}/sdkconfig.partitions)
file(WRITE ${CROWPANEL_PARTITIONS}
    "CONFIG_PARTITION_TABLE_CUSTOM=y\n"
    "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"${MICROPY_BOARD_DIR}/partitions.csv\"\n")
list(APPEND SDKCONFIG_DEFAULTS ${CROWPANEL_PARTITIONS})

# Our own sdkconfig settings (sdkconfig.board)
list(APPEND SDKCONFIG_DEFAULTS ${MICROPY_BOARD_DIR}/sdkconfig.board)
