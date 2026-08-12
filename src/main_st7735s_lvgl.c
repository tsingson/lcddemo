#include "lcd_st7735s.h"

#if ((APP_ENTRY_ST7735S ? 1 : 0) + \
    (APP_ENTRY_ST7735S_LVGL ? 1 : 0) + \
    (APP_ENTRY_ST7789 ? 1 : 0) + \
    (APP_ENTRY_ST7789_LVGL_DEBUG ? 1 : 0)) > 1
#error "Enable only one demo entry via APP_DEMO_ENTRY"
#endif

#if APP_ENTRY_ST7735S_LVGL

#if !APP_ENABLE_LVGL_DEMO
#error "st7735s_lvgl entry requires APP_USE_LVGL_DEMO=ON"
#endif

int main(void)
{
    return lcd_st7735s_demo_run();
}

#endif
