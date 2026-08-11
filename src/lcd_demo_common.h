#ifndef LCD_DEMO_COMMON_H_
#define LCD_DEMO_COMMON_H_

#include <stdint.h>

struct lcd_demo_profile {
    const char *panel_name;
    const char *subtitle;
    uint16_t counter_color;
    uint32_t fill_block_rows;
    const uint8_t *image_data;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_buf_size;
};

/* Run the shared demo pipeline using panel-specific profile settings. */
int lcd_demo_common_run(const struct lcd_demo_profile *profile);

#endif /* LCD_DEMO_COMMON_H_ */