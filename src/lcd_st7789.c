#include "lcd_st7789.h"
#include "lcd_demo_common.h"
#include "image_1_240x320_rgb565.h"
#include "image_2_240x320_rgb565.h"

enum {
    ST7789_SWITCH_PERIOD_MS = 2000,
    ST7789_STARTUP_WHITE_MS = 80,
    ST7789_SUBTITLE_FRAME_MS = 80,
    ST7789_SUBTITLE_STEP_PX = 2,
};

static const struct lcd_demo_profile st7789_profile = {
    .panel_name = "ST7789",
    .title = "\xE5\x8A\xA8\xE6\x80\x81\xE5\xAD\x97\xE5\xB9\x95 Dynamic Caption",
    .subtitle = "\xE4\xB8\x8A\xE4\xB8\x8B\xE8\xB7\x91\xE5\x8A\xA8 / Bilingual Overlay",
    .counter_prefix = "\xE8\xAE\xA1\xE6\x95\xB0 Count:",
    .title_color = 0x0000,
    .subtitle_color = 0x0000,
    .counter_color = 0x0000,
    .bg_color = 0x0000,
    .fill_block_rows = 32U,
    .counter_period_ms = 400U,
    .char_spacing = 1U,
    .line_spacing = 2U,
    .image_data = src_image_1_240x320_rgb565,
    .image_width = 240U,
    .image_height = 320U,
    .image_buf_size = 240U * 320U * 2U,
    .image_data_alt = src_image_2_240x320_rgb565,
    .image_alt_width = 240U,
    .image_alt_height = 320U,
    .image_alt_buf_size = 240U * 320U * 2U,
    .image_switch_period_ms = ST7789_SWITCH_PERIOD_MS,
    .startup_white_hold_ms = ST7789_STARTUP_WHITE_MS,
    .subtitle_frame_ms = ST7789_SUBTITLE_FRAME_MS,
    .subtitle_step_px = ST7789_SUBTITLE_STEP_PX,
    .image_slideshow_only = 0U,
};

int lcd_st7789_demo_run(void)
{
    return lcd_demo_common_run(&st7789_profile);
}