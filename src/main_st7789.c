#include "lcd_st7789.h"

#if ((APP_ENTRY_ST7735S ? 1 : 0) + \
    (APP_ENTRY_ST7789 ? 1 : 0) + \
    (APP_ENTRY_ST7789_LVGL_DEBUG ? 1 : 0)) > 1
#error "Enable only one demo entry via APP_DEMO_ENTRY"
#endif

#if APP_ENTRY_ST7789 && !APP_ENTRY_ST7789_LVGL_DEBUG
int main(void)
{
    return lcd_st7789_demo_run();
}

#endif
