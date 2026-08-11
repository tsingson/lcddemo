#include "lcd_st7789.h"
#include "lcd_demo_common.h"
#include "image_1_240x320_rgb565.h"

static const struct lcd_demo_profile st7789_profile = {
    .panel_name = "ST7789",
    .subtitle = "ST7789 RGB565",
    .counter_color = 0xF81F,
    .fill_block_rows = 32U,
    .image_data = src_image_1_240x320_rgb565,
    .image_width = 240U,
    .image_height = 320U,
    .image_buf_size = 240U * 320U * 2U,
};

int lcd_st7789_demo_run(void)
{
    return lcd_demo_common_run(&st7789_profile);
}