#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#if APP_ENABLE_LVGL_DEMO && defined(CONFIG_LVGL)
#include <lvgl.h>
#endif

#include "lcd_demo_common.h"
#include "zpix12_font_data.h"

LOG_MODULE_REGISTER(lcd_demo_common, LOG_LEVEL_INF);

#define ZPIX_GLYPH_W 12U
#define ZPIX_GLYPH_H 12U
#define COUNTER_BUF_LEN 64U
#define TEXT_MIX_BUF_LEN 256U
#define MAX_BLOCK_WIDTH 320U
#define MAX_FILL_BLOCK_ROWS 32U

#define DEFAULT_FILL_BLOCK_ROWS 24U
#define DEFAULT_BG_COLOR 0x0000U
#define DEFAULT_TITLE_COLOR 0xFFE0U
#define DEFAULT_SUBTITLE_COLOR 0x07FFU
#define DEFAULT_COUNTER_COLOR 0xF81FU
#define DEFAULT_COUNTER_PERIOD_MS 400U
#define DEFAULT_CHAR_SPACING 1U
#define DEFAULT_LINE_SPACING 2U

static const char default_title_text[] =
    "\xE4\xB8\xAD\xE6\x96\x87\xE6\x98\xBE\xE7\xA4\xBA\xE6\xB5\x8B\xE8\xAF\x95 Display Demo";
static const char default_subtitle_text[] =
    "\xE6\x94\xAF\xE6\x8C\x81\xE4\xB8\xAD\xE8\x8B\xB1\xE6\x96\x87\xE8\x87\xAA\xE5\x8A\xA8\xE6\x8A\x98\xE8\xA1\x8C Auto wrap";
static const char default_counter_prefix[] =
    "\xE8\xAE\xA1\xE6\x95\xB0 Count:";

struct image_region {
    const uint8_t *data;
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
};

static uint32_t profile_fill_rows(const struct lcd_demo_profile *profile)
{
    if (profile->fill_block_rows == 0U) {
        return DEFAULT_FILL_BLOCK_ROWS;
    }

    return MIN(profile->fill_block_rows, MAX_FILL_BLOCK_ROWS);
}

static uint16_t profile_bg_color(const struct lcd_demo_profile *profile)
{
    return profile->bg_color;
}

static uint16_t profile_title_color(const struct lcd_demo_profile *profile)
{
    return (profile->title_color == 0U) ? DEFAULT_TITLE_COLOR : profile->title_color;
}

static uint16_t profile_subtitle_color(const struct lcd_demo_profile *profile)
{
    return (profile->subtitle_color == 0U) ? DEFAULT_SUBTITLE_COLOR : profile->subtitle_color;
}

static uint16_t profile_counter_color(const struct lcd_demo_profile *profile)
{
    return (profile->counter_color == 0U) ? DEFAULT_COUNTER_COLOR : profile->counter_color;
}

static uint32_t profile_counter_period_ms(const struct lcd_demo_profile *profile)
{
    return (profile->counter_period_ms == 0U) ? DEFAULT_COUNTER_PERIOD_MS : profile->counter_period_ms;
}

static uint32_t profile_char_spacing(const struct lcd_demo_profile *profile)
{
    return MIN((uint32_t)profile->char_spacing, 8U);
}

static uint32_t profile_line_spacing(const struct lcd_demo_profile *profile)
{
    return MIN((uint32_t)profile->line_spacing, 16U);
}

static const char *profile_title_text(const struct lcd_demo_profile *profile)
{
    return (profile->title != NULL) ? profile->title : default_title_text;
}

static const char *profile_subtitle_text(const struct lcd_demo_profile *profile)
{
    return (profile->subtitle != NULL) ? profile->subtitle : default_subtitle_text;
}

static const char *profile_counter_prefix_text(const struct lcd_demo_profile *profile)
{
    return (profile->counter_prefix != NULL) ? profile->counter_prefix : default_counter_prefix;
}

/* Glyph table is sorted by codepoint, so binary search is predictable and fast. */
static const zpix12_glyph_t *zpix_find_glyph(uint32_t codepoint)
{
    size_t left = 0U;
    size_t right = zpix12_font_glyphs_count;

    while (left < right) {
        const size_t mid = left + ((right - left) / 2U);
        const uint32_t cp = zpix12_font_glyphs[mid].codepoint;

        if (cp == codepoint) {
            return &zpix12_font_glyphs[mid];
        }

        if (cp < codepoint) {
            left = mid + 1U;
        } else {
            right = mid;
        }
    }

    return NULL;
}

static bool utf8_decode_next(const char **text, uint32_t *codepoint)
{
    const uint8_t *s = (const uint8_t *)*text;

    if (*s == 0U) {
        return false;
    }

    if ((s[0] & 0x80U) == 0U) {
        *codepoint = s[0];
        *text += 1;
        return true;
    }

    if ((s[0] & 0xE0U) == 0xC0U && (s[1] & 0xC0U) == 0x80U) {
        *codepoint = ((uint32_t)(s[0] & 0x1FU) << 6U) |
                     (uint32_t)(s[1] & 0x3FU);
        *text += 2;
        return true;
    }

    if ((s[0] & 0xF0U) == 0xE0U &&
        (s[1] & 0xC0U) == 0x80U &&
        (s[2] & 0xC0U) == 0x80U) {
        *codepoint = ((uint32_t)(s[0] & 0x0FU) << 12U) |
                     ((uint32_t)(s[1] & 0x3FU) << 6U) |
                     (uint32_t)(s[2] & 0x3FU);
        *text += 3;
        return true;
    }

    if ((s[0] & 0xF8U) == 0xF0U &&
        (s[1] & 0xC0U) == 0x80U &&
        (s[2] & 0xC0U) == 0x80U &&
        (s[3] & 0xC0U) == 0x80U) {
        *codepoint = ((uint32_t)(s[0] & 0x07U) << 18U) |
                     ((uint32_t)(s[1] & 0x3FU) << 12U) |
                     ((uint32_t)(s[2] & 0x3FU) << 6U) |
                     (uint32_t)(s[3] & 0x3FU);
        *text += 4;
        return true;
    }

    *codepoint = '?';
    *text += 1;
    return true;
}

/* Chunked fill reduces stack use and supports displays wider than MAX_BLOCK_WIDTH. */
static int fill_rect(const struct device *display,
                     uint32_t x,
                     uint32_t y,
                     uint32_t w,
                     uint32_t h,
                     uint16_t rgb565,
                     uint32_t fill_block_rows)
{
    static uint16_t blockbuf[MAX_BLOCK_WIDTH * MAX_FILL_BLOCK_ROWS];
    const uint16_t be = sys_cpu_to_be16(rgb565);
    uint32_t col_off = 0U;

    if (w == 0U || h == 0U) {
        return 0;
    }

    if (fill_block_rows == 0U || fill_block_rows > MAX_FILL_BLOCK_ROWS) {
        return -EINVAL;
    }

    while (col_off < w) {
        const uint32_t chunk_w = MIN(MAX_BLOCK_WIDTH, w - col_off);

        for (uint32_t i = 0U; i < (chunk_w * fill_block_rows); ++i) {
            blockbuf[i] = be;
        }

        uint32_t row = 0U;
        while (row < h) {
            const uint32_t chunk_h = MIN(fill_block_rows, h - row);
            struct display_buffer_descriptor desc = {
                .buf_size = chunk_w * chunk_h * sizeof(uint16_t),
                .width = chunk_w,
                .height = chunk_h,
                .pitch = chunk_w,
            };

            const int ret = display_write(display, x + col_off, y + row, &desc, blockbuf);
            if (ret != 0) {
                return ret;
            }

            row += chunk_h;
        }

        col_off += chunk_w;
    }

    return 0;
}

static int draw_glyph(const struct device *display,
                      uint32_t x,
                      uint32_t y,
                      uint16_t fg,
                      uint16_t bg,
                      const zpix12_glyph_t *glyph)
{
    struct display_buffer_descriptor desc = {
        .buf_size = ZPIX_GLYPH_W * ZPIX_GLYPH_H * sizeof(uint16_t),
        .width = ZPIX_GLYPH_W,
        .height = ZPIX_GLYPH_H,
        .pitch = ZPIX_GLYPH_W,
    };
    uint16_t glyphbuf[ZPIX_GLYPH_W * ZPIX_GLYPH_H];
    const uint16_t fg_be = sys_cpu_to_be16(fg);
    const uint16_t bg_be = sys_cpu_to_be16(bg);

    for (uint32_t row = 0U; row < ZPIX_GLYPH_H; ++row) {
        const uint16_t bits = glyph->rows[row];
        for (uint32_t col = 0U; col < ZPIX_GLYPH_W; ++col) {
            const uint16_t mask = (uint16_t)(1U << (11U - col));
            glyphbuf[row * ZPIX_GLYPH_W + col] = ((bits & mask) != 0U) ? fg_be : bg_be;
        }
    }

    return display_write(display, x, y, &desc, glyphbuf);
}

static uint16_t image_pixel_be(const uint8_t *image_data,
                               uint32_t image_width,
                               uint32_t x,
                               uint32_t y)
{
    const size_t idx = ((size_t)y * image_width + x) * 2U;
    const uint16_t pixel = ((uint16_t)image_data[idx] << 8U) | (uint16_t)image_data[idx + 1U];

    return sys_cpu_to_be16(pixel);
}

static int draw_glyph_image_bg(const struct device *display,
                               uint32_t x,
                               uint32_t y,
                               uint16_t fg,
                               const zpix12_glyph_t *glyph,
                               const struct image_region *image)
{
    struct display_buffer_descriptor desc = {
        .buf_size = ZPIX_GLYPH_W * ZPIX_GLYPH_H * sizeof(uint16_t),
        .width = ZPIX_GLYPH_W,
        .height = ZPIX_GLYPH_H,
        .pitch = ZPIX_GLYPH_W,
    };
    uint16_t glyphbuf[ZPIX_GLYPH_W * ZPIX_GLYPH_H];
    const uint16_t fg_be = sys_cpu_to_be16(fg);
    const uint16_t black_be = sys_cpu_to_be16(0x0000);

    for (uint32_t row = 0U; row < ZPIX_GLYPH_H; ++row) {
        const uint16_t bits = glyph->rows[row];
        for (uint32_t col = 0U; col < ZPIX_GLYPH_W; ++col) {
            const uint16_t mask = (uint16_t)(1U << (11U - col));
            if ((bits & mask) != 0U) {
                glyphbuf[row * ZPIX_GLYPH_W + col] = fg_be;
                continue;
            }

            const uint32_t sx = x + col;
            const uint32_t sy = y + row;
            if (sx >= image->x && sy >= image->y &&
                (sx - image->x) < image->width &&
                (sy - image->y) < image->height) {
                glyphbuf[row * ZPIX_GLYPH_W + col] = image_pixel_be(image->data,
                                                                     image->width,
                                                                     sx - image->x,
                                                                     sy - image->y);
            } else {
                glyphbuf[row * ZPIX_GLYPH_W + col] = black_be;
            }
        }
    }

    return display_write(display, x, y, &desc, glyphbuf);
}

static int draw_text_utf8_wrapped(const struct device *display,
                                  uint32_t x,
                                  uint32_t y,
                                  uint32_t max_width,
                                  uint32_t max_height,
                                  uint16_t fg,
                                  uint16_t bg,
                                  const char *text,
                                  uint32_t char_spacing,
                                  uint32_t line_spacing,
                                  uint32_t *next_line_y)
{
    const char *p = text;
    const uint32_t line_advance = ZPIX_GLYPH_H + line_spacing;
    const uint32_t x_end = x + max_width;
    const uint32_t y_end = y + max_height;
    uint32_t cursor_x = x;
    uint32_t cursor_y = y;

    if (text == NULL || max_width == 0U || max_height < ZPIX_GLYPH_H) {
        return -EINVAL;
    }

    while (*p != '\0') {
        uint32_t codepoint = 0U;

        if (!utf8_decode_next(&p, &codepoint)) {
            break;
        }

        if (codepoint == '\n') {
            cursor_x = x;
            cursor_y += line_advance;
            if (cursor_y + ZPIX_GLYPH_H > y_end) {
                break;
            }
            continue;
        }

        const zpix12_glyph_t *glyph = zpix_find_glyph(codepoint);
        if (glyph == NULL) {
            glyph = zpix_find_glyph('?');
            if (glyph == NULL) {
                continue;
            }
        }

        if (cursor_x + ZPIX_GLYPH_W > x_end) {
            cursor_x = x;
            cursor_y += line_advance;
            if (cursor_y + ZPIX_GLYPH_H > y_end) {
                break;
            }
        }

        const int ret = draw_glyph(display, cursor_x, cursor_y, fg, bg, glyph);
        if (ret != 0) {
            return ret;
        }

        cursor_x += ZPIX_GLYPH_W;
        if (cursor_x + char_spacing < x_end) {
            cursor_x += char_spacing;
        }
    }

    if (next_line_y != NULL) {
        *next_line_y = MIN(cursor_y + line_advance, y_end);
    }

    return 0;
}

static int draw_text_utf8_wrapped_image_bg(const struct device *display,
                                           uint32_t x,
                                           uint32_t y,
                                           uint32_t max_width,
                                           uint32_t max_height,
                                           uint16_t fg,
                                           const char *text,
                                           uint32_t char_spacing,
                                           uint32_t line_spacing,
                                           const struct image_region *image,
                                           uint32_t *next_line_y)
{
    const char *p = text;
    const uint32_t line_advance = ZPIX_GLYPH_H + line_spacing;
    const uint32_t x_end = x + max_width;
    const uint32_t y_end = y + max_height;
    uint32_t cursor_x = x;
    uint32_t cursor_y = y;

    if (text == NULL || image == NULL || max_width == 0U || max_height < ZPIX_GLYPH_H) {
        return -EINVAL;
    }

    while (*p != '\0') {
        uint32_t codepoint = 0U;

        if (!utf8_decode_next(&p, &codepoint)) {
            break;
        }

        if (codepoint == '\n') {
            cursor_x = x;
            cursor_y += line_advance;
            if (cursor_y + ZPIX_GLYPH_H > y_end) {
                break;
            }
            continue;
        }

        const zpix12_glyph_t *glyph = zpix_find_glyph(codepoint);
        if (glyph == NULL) {
            glyph = zpix_find_glyph('?');
            if (glyph == NULL) {
                continue;
            }
        }

        if (cursor_x + ZPIX_GLYPH_W > x_end) {
            cursor_x = x;
            cursor_y += line_advance;
            if (cursor_y + ZPIX_GLYPH_H > y_end) {
                break;
            }
        }

        const int ret = draw_glyph_image_bg(display, cursor_x, cursor_y, fg, glyph, image);
        if (ret != 0) {
            return ret;
        }

        cursor_x += ZPIX_GLYPH_W;
        if (cursor_x + char_spacing < x_end) {
            cursor_x += char_spacing;
        }
    }

    if (next_line_y != NULL) {
        *next_line_y = MIN(cursor_y + line_advance, y_end);
    }

    return 0;
}

static int restore_region_from_image(const struct device *display,
                                     uint32_t dst_x,
                                     uint32_t dst_y,
                                     uint32_t width,
                                     uint32_t height,
                                     const struct image_region *image)
{
    static uint16_t linebuf[MAX_BLOCK_WIDTH];
    const uint16_t black_be = sys_cpu_to_be16(0x0000);

    if (width == 0U || height == 0U) {
        return 0;
    }

    for (uint32_t row = 0U; row < height; ++row) {
        const uint32_t sy = dst_y + row;
        uint32_t col_off = 0U;

        while (col_off < width) {
            const uint32_t chunk_w = MIN(MAX_BLOCK_WIDTH, width - col_off);

            for (uint32_t col = 0U; col < chunk_w; ++col) {
                const uint32_t sx = dst_x + col_off + col;

                if (sx >= image->x && sy >= image->y &&
                    (sx - image->x) < image->width &&
                    (sy - image->y) < image->height) {
                    linebuf[col] = image_pixel_be(image->data,
                                                  image->width,
                                                  sx - image->x,
                                                  sy - image->y);
                } else {
                    linebuf[col] = black_be;
                }
            }

            struct display_buffer_descriptor desc = {
                .buf_size = chunk_w * sizeof(uint16_t),
                .width = chunk_w,
                .height = 1U,
                .pitch = chunk_w,
            };

            const int ret = display_write(display, dst_x + col_off, sy, &desc, linebuf);
            if (ret != 0) {
                return ret;
            }

            col_off += chunk_w;
        }
    }

    return 0;
}

#if APP_ENABLE_LVGL_DEMO && defined(CONFIG_LVGL)
struct lvgl_demo_ctx {
    lv_obj_t *counter_label;
    lv_obj_t *heartbeat_dot;
    uint32_t counter;
    bool dot_on;
};

static void lvgl_counter_timer_cb(lv_timer_t *timer)
{
    struct lvgl_demo_ctx *ctx = (struct lvgl_demo_ctx *)lv_timer_get_user_data(timer);

    if (ctx == NULL || ctx->counter_label == NULL || ctx->heartbeat_dot == NULL) {
        return;
    }

    ++ctx->counter;
    lv_label_set_text_fmt(ctx->counter_label, "Count: %lu", (unsigned long)ctx->counter);

    ctx->dot_on = !ctx->dot_on;
    lv_obj_set_style_bg_color(ctx->heartbeat_dot,
                              lv_color_hex(ctx->dot_on ? 0xFF3B30 : 0x1E90FF),
                              0);

    if ((ctx->counter % 10U) == 0U) {
        LOG_INF("LVGL timer ticks: %lu", (unsigned long)ctx->counter);
    }
}

static int run_lvgl_demo(const struct lcd_demo_profile *profile)
{
    lv_obj_t *scr;
    lv_obj_t *label;
    lv_obj_t *counter_label;
    lv_obj_t *heartbeat_dot;
    lv_timer_t *counter_timer;
    lv_display_t *lv_disp;
    static struct lvgl_demo_ctx ctx;
    ARG_UNUSED(profile);

    /* LVGL display object is created by Zephyr LVGL glue when CONFIG_LVGL is enabled. */
    lv_disp = lv_display_get_default();
    if (lv_disp == NULL) {
        LOG_ERR("LVGL default display is NULL");
        return -ENODEV;
    }

    LOG_INF("LVGL default display is ready");

    scr = lv_scr_act();
    if (scr == NULL) {
        LOG_ERR("LVGL active screen is NULL");
        return -ENODEV;
    }

    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x00FF00), 0);

    label = lv_label_create(scr);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, "LVGL OK\nST7789 ACTIVE");
    lv_obj_set_style_text_color(label, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 40);

    counter_label = lv_label_create(scr);
    lv_label_set_text(counter_label, "Count: 0");
    lv_obj_set_style_text_color(counter_label, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_font(counter_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(counter_label, LV_ALIGN_CENTER, 0, 0);

    heartbeat_dot = lv_obj_create(scr);
    lv_obj_remove_style_all(heartbeat_dot);
    lv_obj_set_size(heartbeat_dot, 16, 16);
    lv_obj_set_style_radius(heartbeat_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(heartbeat_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(heartbeat_dot, lv_color_hex(0x1E90FF), 0);
    lv_obj_align(heartbeat_dot, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

    ctx.counter_label = counter_label;
    ctx.heartbeat_dot = heartbeat_dot;
    ctx.counter = 0U;
    ctx.dot_on = false;

    /* Use timer + user_data to prove dynamic updates are coming from LVGL itself. */
    counter_timer = lv_timer_create(lvgl_counter_timer_cb, 500U, &ctx);
    if (counter_timer == NULL) {
        LOG_ERR("LVGL timer create failed");
        return -ENOMEM;
    }

    LOG_INF("LVGL demo UI created");
    LOG_INF("LVGL timer created");
    lv_refr_now(NULL);
    LOG_INF("LVGL refresh requested");

    uint32_t tick = 0U;
    while (1) {
        /* Typical Zephyr+LVGL pump: run handlers then sleep for a short slice. */
        lv_timer_handler();
        if ((tick % 25U) == 0U) {
            LOG_INF("LVGL loop alive: %u", tick);
        }
        ++tick;
        k_msleep(20);
    }

    return 0;
}
#endif

int lcd_demo_common_run(const struct lcd_demo_profile *profile)
{
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    struct display_capabilities caps;
    const uint16_t colors[] = {
        0xF800,
        0x07E0,
        0x001F,
        0xFFFF,
        0x0000,
    };
    char counter_text[COUNTER_BUF_LEN];
    char text_mix[TEXT_MIX_BUF_LEN];
    uint32_t counter = 0U;
    uint32_t next_y = 0U;

    if (profile == NULL || profile->panel_name == NULL) {
        return -EINVAL;
    }

    if (!device_is_ready(display)) {
        LOG_ERR("Display device is not ready");
        return -ENODEV;
    }

    display_get_capabilities(display, &caps);
    LOG_INF("%s demo: %ux%u", profile->panel_name, caps.x_resolution, caps.y_resolution);

    int ret = display_set_pixel_format(display, PIXEL_FORMAT_RGB_565);
    if (ret != 0) {
        LOG_ERR("display_set_pixel_format failed: %d", ret);
        return ret;
    }

    ret = display_blanking_off(display);
    if (ret != 0) {
        LOG_ERR("display_blanking_off failed: %d", ret);
        return ret;
    }

    const uint32_t width = caps.x_resolution;
    const uint32_t height = caps.y_resolution;
    const uint32_t fill_rows = profile_fill_rows(profile);

    /* Force a deterministic first frame after power-up to suppress random pixels. */
    ret = fill_rect(display, 0U, 0U, width, height, 0xFFFFU, fill_rows);
    if (ret != 0) {
        LOG_ERR("startup white clear failed: %d", ret);
        return ret;
    }

    k_msleep(profile->startup_white_hold_ms);

#if APP_ENABLE_LVGL_DEMO && defined(CONFIG_LVGL)
    /* Build-time app switch: route to dedicated LVGL debug path. */
    return run_lvgl_demo(profile);
#endif

    const uint32_t char_spacing = profile_char_spacing(profile);
    const uint32_t line_spacing = profile_line_spacing(profile);
    const uint32_t counter_period = profile_counter_period_ms(profile);
    const uint16_t bg_color = profile_bg_color(profile);
    const char *title = profile_title_text(profile);
    const char *subtitle = profile_subtitle_text(profile);
    const char *counter_prefix = profile_counter_prefix_text(profile);

    if (profile->image_data != NULL) {
        struct image_region image;
        const size_t expected_image_size = (size_t)profile->image_width * (size_t)profile->image_height * 2U;
        const bool has_alt_image = (profile->image_data_alt != NULL);
        struct image_region image_alt;
        struct display_buffer_descriptor image_desc_alt;
        size_t expected_image_alt_size = 0U;
        const uint32_t switch_period_ms =
            (profile->image_switch_period_ms == 0U) ? 2000U : profile->image_switch_period_ms;
        const uint32_t box_margin_x = 2U;
        const uint32_t box_margin_y = 2U;
        const uint32_t counter_line_h = ZPIX_GLYPH_H + line_spacing;
        const uint32_t text_box_h = MIN(height, (ZPIX_GLYPH_H * 4U) + (line_spacing * 3U) + (box_margin_y * 2U));
        const uint32_t text_box_y = height - text_box_h;
        const uint32_t text_x = box_margin_x;
        const uint32_t text_y = text_box_y + box_margin_y;
        const uint32_t text_w = (width > box_margin_x * 2U) ? (width - box_margin_x * 2U) : width;
        const uint32_t counter_y = (height > (box_margin_y + ZPIX_GLYPH_H)) ?
                                   (height - box_margin_y - ZPIX_GLYPH_H) :
                                   text_y;
        const uint32_t body_h = (counter_y > text_y + 1U) ? (counter_y - text_y - 1U) : ZPIX_GLYPH_H;

        if (profile->image_width == 0U || profile->image_height == 0U) {
            LOG_ERR("image dimensions cannot be zero");
            return -EINVAL;
        }

        if (profile->image_width > width || profile->image_height > height) {
            LOG_ERR("image dimensions exceed panel size");
            return -EINVAL;
        }

        if ((size_t)profile->image_buf_size < expected_image_size) {
            LOG_ERR("image buffer size mismatch");
            return -EINVAL;
        }

        if (has_alt_image) {
            if (profile->image_alt_width == 0U || profile->image_alt_height == 0U) {
                LOG_ERR("alt image dimensions cannot be zero");
                return -EINVAL;
            }

            if (profile->image_alt_width > width || profile->image_alt_height > height) {
                LOG_ERR("alt image dimensions exceed panel size");
                return -EINVAL;
            }

            expected_image_alt_size =
                (size_t)profile->image_alt_width * (size_t)profile->image_alt_height * 2U;
            if ((size_t)profile->image_alt_buf_size < expected_image_alt_size) {
                LOG_ERR("alt image buffer size mismatch");
                return -EINVAL;
            }
        }

        ret = fill_rect(display, 0U, 0U, width, height, bg_color, fill_rows);
        if (ret != 0) {
            LOG_ERR("clear screen failed: %d", ret);
            return ret;
        }

        image.data = profile->image_data;
        image.width = profile->image_width;
        image.height = profile->image_height;
        image.x = (width - image.width) / 2U;
        image.y = (height - image.height) / 2U;

        struct display_buffer_descriptor image_desc = {
            .buf_size = profile->image_buf_size,
            .width = image.width,
            .height = image.height,
            .pitch = image.width,
        };

        if (has_alt_image) {
            image_alt.data = profile->image_data_alt;
            image_alt.width = profile->image_alt_width;
            image_alt.height = profile->image_alt_height;
            image_alt.x = (width - image_alt.width) / 2U;
            image_alt.y = (height - image_alt.height) / 2U;

            image_desc_alt.buf_size = profile->image_alt_buf_size;
            image_desc_alt.width = image_alt.width;
            image_desc_alt.height = image_alt.height;
            image_desc_alt.pitch = image_alt.width;
        }

        if (profile->image_slideshow_only != 0U) {
            bool show_primary = true;
            const uint32_t text_margin_x = 2U;
            const uint32_t text_w = (width > text_margin_x * 2U) ? (width - text_margin_x * 2U) : width;
            const uint32_t line_advance = ZPIX_GLYPH_H + line_spacing;
            const uint32_t subtitle_h = (line_advance * 2U) + ZPIX_GLYPH_H;
            const uint32_t frame_ms = (profile->subtitle_frame_ms == 0U) ? 80U : profile->subtitle_frame_ms;
            int32_t subtitle_y = (int32_t)(height / 3U);
            int32_t subtitle_step = (profile->subtitle_step_px == 0U) ? 2 : (int32_t)profile->subtitle_step_px;
            int32_t min_y = 4;
            int32_t max_y = (int32_t)height - (int32_t)subtitle_h - 4;
            int32_t prev_subtitle_y = -1;

            if (max_y < min_y) {
                min_y = 0;
                max_y = 0;
                subtitle_y = 0;
            }

            snprintk(text_mix,
                     sizeof(text_mix),
                     "%s\n%s",
                     profile_title_text(profile),
                     profile_subtitle_text(profile));

            while (1) {
                const struct image_region *active_image = &image;
                const struct display_buffer_descriptor *active_desc = &image_desc;
                uint32_t elapsed_ms = 0U;

                if (has_alt_image && !show_primary) {
                    active_image = &image_alt;
                    active_desc = &image_desc_alt;
                }

                ret = display_write(display,
                                    active_image->x,
                                    active_image->y,
                                    active_desc,
                                    active_image->data);
                if (ret != 0) {
                    LOG_ERR("draw slideshow image failed: %d", ret);
                    return ret;
                }

                if (has_alt_image) {
                    show_primary = !show_primary;
                }

                prev_subtitle_y = -1;
                while (elapsed_ms < switch_period_ms) {
                    /* Restore previous text area from source image before drawing next frame. */
                    if (prev_subtitle_y >= 0) {
                        ret = restore_region_from_image(display,
                                                        0U,
                                                        (uint32_t)prev_subtitle_y,
                                                        width,
                                                        subtitle_h,
                                                        active_image);
                        if (ret != 0) {
                            LOG_ERR("restore slideshow subtitle failed: %d", ret);
                            return ret;
                        }
                    }

                    ret = draw_text_utf8_wrapped_image_bg(display,
                                                          text_margin_x,
                                                          (uint32_t)subtitle_y,
                                                          text_w,
                                                          subtitle_h,
                                                          0x0000U,
                                                          text_mix,
                                                          char_spacing,
                                                          line_spacing,
                                                          active_image,
                                                          NULL);
                    if (ret != 0) {
                        LOG_ERR("draw slideshow subtitle failed: %d", ret);
                        return ret;
                    }

                    prev_subtitle_y = subtitle_y;
                    subtitle_y += subtitle_step;
                    /* Bounce between top and bottom limits for vertical marquee motion. */
                    if (subtitle_y <= min_y) {
                        subtitle_y = min_y;
                        subtitle_step = -subtitle_step;
                    } else if (subtitle_y >= max_y) {
                        subtitle_y = max_y;
                        subtitle_step = -subtitle_step;
                    }

                    k_msleep(frame_ms);
                    elapsed_ms += frame_ms;
                }
            }
        }

        ret = display_write(display, image.x, image.y, &image_desc, image.data);
        if (ret != 0) {
            LOG_ERR("draw image failed: %d", ret);
            return ret;
        }

        snprintk(text_mix, sizeof(text_mix), "%s\n%s", title, subtitle);

        while (1) {
            ret = restore_region_from_image(display,
                                            0U,
                                            text_box_y,
                                            width,
                                            text_box_h,
                                            &image);
            if (ret != 0) {
                LOG_ERR("restore text background failed: %d", ret);
                return ret;
            }

            ret = draw_text_utf8_wrapped_image_bg(display,
                                                  text_x,
                                                  text_y,
                                                  text_w,
                                                  body_h,
                                                  profile_title_color(profile),
                                                  text_mix,
                                                  char_spacing,
                                                  line_spacing,
                                                  &image,
                                                  NULL);
            if (ret != 0) {
                LOG_ERR("draw wrapped image text failed: %d", ret);
                return ret;
            }

            snprintk(counter_text, sizeof(counter_text), "%s %lu", counter_prefix, (unsigned long)counter);
            ret = draw_text_utf8_wrapped_image_bg(display,
                                                  text_x,
                                                  counter_y,
                                                  text_w,
                                                  counter_line_h,
                                                  profile_counter_color(profile),
                                                  counter_text,
                                                  char_spacing,
                                                  line_spacing,
                                                  &image,
                                                  NULL);
            if (ret != 0) {
                LOG_ERR("draw wrapped image counter failed: %d", ret);
                return ret;
            }

            ++counter;
            k_msleep(counter_period);
        }
    }

    ret = fill_rect(display, 0U, 0U, width, height, bg_color, fill_rows);
    if (ret != 0) {
        LOG_ERR("clear screen failed: %d", ret);
        return ret;
    }

    const uint32_t margin = 2U;
    const uint32_t text_w = (width > margin * 2U) ? (width - margin * 2U) : width;
    const uint32_t counter_line_h = ZPIX_GLYPH_H + line_spacing;
    const uint32_t header_h = MIN(height / 3U, 96U);
    const uint32_t bars_y = MIN(height, header_h + counter_line_h + 2U);

    ret = draw_text_utf8_wrapped(display,
                                 margin,
                                 margin,
                                 text_w,
                                 header_h,
                                 profile_title_color(profile),
                                 bg_color,
                                 title,
                                 char_spacing,
                                 line_spacing,
                                 &next_y);
    if (ret != 0) {
        LOG_ERR("draw wrapped title failed: %d", ret);
        return ret;
    }

    const uint32_t subtitle_y = MIN(next_y, margin + header_h - ZPIX_GLYPH_H);
    ret = draw_text_utf8_wrapped(display,
                                 margin,
                                 subtitle_y,
                                 text_w,
                                 (margin + header_h > subtitle_y) ? ((margin + header_h) - subtitle_y) : ZPIX_GLYPH_H,
                                 profile_subtitle_color(profile),
                                 bg_color,
                                 subtitle,
                                 char_spacing,
                                 line_spacing,
                                 NULL);
    if (ret != 0) {
        LOG_ERR("draw wrapped subtitle failed: %d", ret);
        return ret;
    }

    while (1) {
        const uint32_t bars_h = (height > bars_y) ? (height - bars_y) : 0U;
        const uint32_t one_bar_h = (bars_h / ARRAY_SIZE(colors)) == 0U ? 1U :
                                   (bars_h / ARRAY_SIZE(colors));

        for (size_t c = 0U; c < ARRAY_SIZE(colors); ++c) {
            const size_t idx = (c + (counter % ARRAY_SIZE(colors))) % ARRAY_SIZE(colors);
            const uint32_t y0 = bars_y + (uint32_t)c * one_bar_h;
            const uint32_t h = (c == ARRAY_SIZE(colors) - 1U) ?
                               (height - y0) : one_bar_h;

            ret = fill_rect(display, 0U, y0, width, h, colors[idx], fill_rows);
            if (ret != 0) {
                LOG_ERR("draw color bar failed: %d", ret);
                return ret;
            }
        }

        const uint32_t counter_y = (bars_y > counter_line_h) ? (bars_y - counter_line_h) : 0U;
        ret = fill_rect(display, 0U, counter_y, width, counter_line_h, bg_color, fill_rows);
        if (ret != 0) {
            LOG_ERR("clear counter area failed: %d", ret);
            return ret;
        }

        snprintk(counter_text, sizeof(counter_text), "%s %lu", counter_prefix, (unsigned long)counter);
        ret = draw_text_utf8_wrapped(display,
                                     margin,
                                     counter_y,
                                     text_w,
                                     counter_line_h,
                                     profile_counter_color(profile),
                                     bg_color,
                                     counter_text,
                                     char_spacing,
                                     line_spacing,
                                     NULL);
        if (ret != 0) {
            LOG_ERR("draw wrapped counter failed: %d", ret);
            return ret;
        }

        ++counter;
        k_msleep(counter_period);
    }
}