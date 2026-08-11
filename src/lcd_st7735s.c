#include "lcd_st7735s.h"
#include "lcd_demo_common.h"

static const struct lcd_demo_profile st7735s_profile = {
    .panel_name = "ST7735S",
    .subtitle = "ST7735S RGB565",
    .counter_color = 0xF81F,
    .fill_block_rows = 24U,
};

int lcd_st7735s_demo_run(void)
{
    return lcd_demo_common_run(&st7735s_profile);
}