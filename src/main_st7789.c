#include "lcd_st7789.h"

#if defined(CONFIG_APP_ST7735S_DEMO) && defined(CONFIG_APP_ST7789_DEMO)
#error "Enable only one demo entry: CONFIG_APP_ST7735S_DEMO or CONFIG_APP_ST7789_DEMO"
#endif

#if defined(CONFIG_APP_ST7789_DEMO)
int main(void)
{
    return lcd_st7789_demo_run();
}

#endif
