cmake_minimum_required(VERSION 3.20.0)
# Application-level demo selection is handled here (not in Kconfig):
# APP_DEMO_ENTRY: st7735s | st7789
set(APP_DEMO_ENTRY "st7735s" CACHE STRING "Demo entry: st7735s|st7789")
set_property(CACHE APP_DEMO_ENTRY PROPERTY STRINGS st7735s st7789)

# Auto-select overlay and extra conf defaults to keep west commands short.
# Users can still override with -DDTC_OVERLAY_FILE=... and -DEXTRA_CONF_FILE=...
if(NOT DEFINED DTC_OVERLAY_FILE)
    if(APP_DEMO_ENTRY STREQUAL "st7735s")
        set(DTC_OVERLAY_FILE "boards/esp32_devkitc_esp32_procpu_st7735s.overlay" CACHE STRING "Devicetree overlay" )
    else()
        set(DTC_OVERLAY_FILE "boards/esp32_devkitc_esp32_procpu_st7789.overlay" CACHE STRING "Devicetree overlay" )
    endif()
endif()

if(NOT DEFINED EXTRA_CONF_FILE)
    if(APP_DEMO_ENTRY STREQUAL "st7789")
        set(EXTRA_CONF_FILE "prj_st7789.conf" CACHE STRING "Extra Kconfig fragment" )
    endif()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(led_spi_display_verify C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(_APP_ENTRY_ST7735S 0)
set(_APP_ENTRY_ST7789 0)

if(APP_DEMO_ENTRY STREQUAL "st7735s")
    set(_APP_ENTRY_ST7735S 1)
elseif(APP_DEMO_ENTRY STREQUAL "st7789")
    set(_APP_ENTRY_ST7789 1)
else()
    message(FATAL_ERROR "Invalid APP_DEMO_ENTRY='${APP_DEMO_ENTRY}'. Use st7735s or st7789.")
endif()

target_compile_definitions(app PRIVATE
    APP_ENTRY_ST7735S=${_APP_ENTRY_ST7735S}
    APP_ENTRY_ST7789=${_APP_ENTRY_ST7789}
)

target_sources(app PRIVATE
    src/image_1_128x160_rgb565.c
    src/image_1_240x320_rgb565.c
    src/image_2_128x160_rgb565.c
    src/image_2_240x320_rgb565.c
    src/lcd_demo_common.c
    src/lcd_st7735s.c
    src/lcd_st7789.c
    src/main_st7735s.c
    src/main_st7789.c
    src/zpix12_font_data.c
)
