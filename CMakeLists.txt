cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(led_spi_display_verify C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Application-level demo selection is handled here (not in Kconfig):
# APP_DEMO_ENTRY: st7735s | st7789 | st7789_lvgl_debug
# APP_USE_LVGL_DEMO: ON/OFF for shared demo LVGL path.
set(APP_DEMO_ENTRY "st7735s" CACHE STRING "Demo entry: st7735s|st7789|st7789_lvgl_debug")
set_property(CACHE APP_DEMO_ENTRY PROPERTY STRINGS st7735s st7789 st7789_lvgl_debug)
option(APP_USE_LVGL_DEMO "Use LVGL path in lcd_demo_common" OFF)

set(_APP_ENTRY_ST7735S 0)
set(_APP_ENTRY_ST7789 0)
set(_APP_ENTRY_ST7789_LVGL_DEBUG 0)

if(APP_DEMO_ENTRY STREQUAL "st7735s")
    set(_APP_ENTRY_ST7735S 1)
elseif(APP_DEMO_ENTRY STREQUAL "st7789")
    set(_APP_ENTRY_ST7789 1)
elseif(APP_DEMO_ENTRY STREQUAL "st7789_lvgl_debug")
    set(_APP_ENTRY_ST7789_LVGL_DEBUG 1)
else()
    message(FATAL_ERROR "Invalid APP_DEMO_ENTRY='${APP_DEMO_ENTRY}'. Use st7735s, st7789, or st7789_lvgl_debug.")
endif()

if(APP_USE_LVGL_DEMO)
    set(_APP_ENABLE_LVGL_DEMO 1)
else()
    set(_APP_ENABLE_LVGL_DEMO 0)
endif()

target_compile_definitions(app PRIVATE
    APP_ENTRY_ST7735S=${_APP_ENTRY_ST7735S}
    APP_ENTRY_ST7789=${_APP_ENTRY_ST7789}
    APP_ENTRY_ST7789_LVGL_DEBUG=${_APP_ENTRY_ST7789_LVGL_DEBUG}
    APP_ENABLE_LVGL_DEMO=${_APP_ENABLE_LVGL_DEMO}
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
    src/main_st7789_lgvl.c
    src/zpix12_font_data.c
)
