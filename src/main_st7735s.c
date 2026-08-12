#include "lcd_st7735s.h"

#if ((APP_ENTRY_ST7735S ? 1 : 0) + \
    (APP_ENTRY_ST7789 ? 1 : 0)) > 1
#error "Enable only one demo entry via APP_DEMO_ENTRY"
#endif

#if APP_ENTRY_ST7735S
int main(void)
{
    return lcd_st7735s_demo_run();
}

#endif
