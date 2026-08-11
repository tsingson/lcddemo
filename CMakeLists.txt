cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(led_spi_display_verify C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

target_sources(app PRIVATE
    src/image_1_240x320_rgb565.c
    src/lcd_demo_common.c
    src/lcd_st7735s.c
    src/lcd_st7789.c
    src/main_st7735s.c
    src/main_st7789.c
    src/zpix12_font_data.c
)
