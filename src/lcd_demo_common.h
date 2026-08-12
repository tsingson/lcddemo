#ifndef LCD_DEMO_COMMON_H_
#define LCD_DEMO_COMMON_H_

#include <stdint.h>

struct lcd_demo_profile {
    /* Identity and text content. */
    const char *panel_name;
    const char *title;
    const char *subtitle;
    const char *counter_prefix;

    /* Text and background colors (RGB565). */
    uint16_t title_color;
    uint16_t subtitle_color;
    uint16_t counter_color;
    uint16_t bg_color;

    /* Shared renderer pacing and layout. */
    uint32_t fill_block_rows;
    uint32_t counter_period_ms;
    uint8_t char_spacing;
    uint8_t line_spacing;
    uint8_t glyph_size_px;

    /* Primary image payload (RGB565 big-endian byte stream). */
    const uint8_t *image_data;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_buf_size;

    /* Optional secondary image for slideshow mode. */
    const uint8_t *image_data_alt;
    uint32_t image_alt_width;
    uint32_t image_alt_height;
    uint32_t image_alt_buf_size;

    /* Slideshow/overlay behavior controls. */
    uint32_t image_switch_period_ms;
    uint32_t startup_white_hold_ms;
    uint32_t subtitle_frame_ms;
    uint8_t subtitle_step_px;
    uint8_t image_slideshow_only;
};

/* Run the shared demo pipeline using panel-specific profile settings. */
int lcd_demo_common_run(const struct lcd_demo_profile *profile);

#endif /* LCD_DEMO_COMMON_H_ */