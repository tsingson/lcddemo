#include <stdint.h>
#include <errno.h>
#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "lcd_demo_common.h"
#include "zpix12_font_data.h"

LOG_MODULE_REGISTER(lcd_demo_common, LOG_LEVEL_INF);

#define ZPIX_GLYPH_W 12U
#define ZPIX_GLYPH_H 12U
#define ZPIX_GLYPH_SPACING 1U
#define COUNTER_BUF_LEN 40U
#define MAX_BLOCK_WIDTH 320U
#define MAX_FILL_BLOCK_ROWS 32U
#define DEFAULT_FILL_BLOCK_ROWS 24U

static const char title_text[] = "\xE4\xB8\xAD\xE6\x96\x87\xE6\x98\xBE\xE7\xA4\xBA\xE6\xB5\x8B\xE8\xAF\x95";
static const char counter_prefix[] = "\xE8\xAE\xA1\xE6\x95\xB0:";

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

/* Chunked fill reduces the number of display_write calls for large areas. */
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

    if (w > MAX_BLOCK_WIDTH || fill_block_rows == 0U || fill_block_rows > MAX_FILL_BLOCK_ROWS) {
        return -EINVAL;
    }

    for (uint32_t i = 0U; i < (w * fill_block_rows); ++i) {
        blockbuf[i] = be;
    }

    uint32_t row = 0U;
    while (row < h) {
        const uint32_t chunk_h = MIN(fill_block_rows, h - row);
        struct display_buffer_descriptor desc = {
            .buf_size = w * chunk_h * sizeof(uint16_t),
            .width = w,
            .height = chunk_h,
            .pitch = w,
        };

        const int ret = display_write(display, x, y + row, &desc, blockbuf);
        if (ret != 0) {
            return ret;
        }

        row += chunk_h;
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

    return ((uint16_t)image_data[idx] << 8U) | (uint16_t)image_data[idx + 1U];
}

static int draw_glyph_image_bg(const struct device *display,
                               uint32_t x,
                               uint32_t y,
                               uint16_t fg,
                               const zpix12_glyph_t *glyph,
                               const uint8_t *image_data,
                               uint32_t image_width,
                               uint32_t image_height,
                               uint32_t image_x,
                               uint32_t image_y)
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
            if (sx >= image_x && sy >= image_y &&
                (sx - image_x) < image_width &&
                (sy - image_y) < image_height) {
                glyphbuf[row * ZPIX_GLYPH_W + col] = image_pixel_be(image_data,
                                                                     image_width,
                                                                     sx - image_x,
                                                                     sy - image_y);
            } else {
                glyphbuf[row * ZPIX_GLYPH_W + col] = black_be;
            }
        }
    }

    return display_write(display, x, y, &desc, glyphbuf);
}

static int draw_text_utf8(const struct device *display,
                          uint32_t x,
                          uint32_t y,
                          uint16_t fg,
                          uint16_t bg,
                          const char *text,
                          uint32_t max_width)
{
    const char *p = text;
    uint32_t cursor = x;

    while (*p != '\0') {
        uint32_t codepoint = 0U;
        if (!utf8_decode_next(&p, &codepoint)) {
            break;
        }

        const zpix12_glyph_t *glyph = zpix_find_glyph(codepoint);
        if (glyph == NULL) {
            glyph = zpix_find_glyph('?');
            if (glyph == NULL) {
                continue;
            }
        }

        if (cursor + ZPIX_GLYPH_W > max_width) {
            break;
        }

        const int ret = draw_glyph(display, cursor, y, fg, bg, glyph);
        if (ret != 0) {
            return ret;
        }

        cursor += ZPIX_GLYPH_W + ZPIX_GLYPH_SPACING;
    }

    return 0;
}

static int draw_text_utf8_image_bg(const struct device *display,
                                   uint32_t x,
                                   uint32_t y,
                                   uint16_t fg,
                                   const char *text,
                                   uint32_t max_width,
                                   const uint8_t *image_data,
                                   uint32_t image_width,
                                   uint32_t image_height,
                                   uint32_t image_x,
                                   uint32_t image_y)
{
    const char *p = text;
    uint32_t cursor = x;

    while (*p != '\0') {
        uint32_t codepoint = 0U;
        if (!utf8_decode_next(&p, &codepoint)) {
            break;
        }

        const zpix12_glyph_t *glyph = zpix_find_glyph(codepoint);
        if (glyph == NULL) {
            glyph = zpix_find_glyph('?');
            if (glyph == NULL) {
                continue;
            }
        }

        if (cursor + ZPIX_GLYPH_W > max_width) {
            break;
        }

        const int ret = draw_glyph_image_bg(display,
                                            cursor,
                                            y,
                                            fg,
                                            glyph,
                                            image_data,
                                            image_width,
                                            image_height,
                                            image_x,
                                            image_y);
        if (ret != 0) {
            return ret;
        }

        cursor += ZPIX_GLYPH_W + ZPIX_GLYPH_SPACING;
    }

    return 0;
}

static int restore_region_from_image(const struct device *display,
                                     uint32_t dst_x,
                                     uint32_t dst_y,
                                     uint32_t width,
                                     uint32_t height,
                                     const uint8_t *image_data,
                                     uint32_t image_width,
                                     uint32_t image_height,
                                     uint32_t image_x,
                                     uint32_t image_y)
{
    static uint16_t linebuf[MAX_BLOCK_WIDTH];
    const uint16_t black_be = sys_cpu_to_be16(0x0000);

    if (width == 0U || height == 0U) {
        return 0;
    }

    if (width > MAX_BLOCK_WIDTH) {
        return -EINVAL;
    }

    for (uint32_t row = 0U; row < height; ++row) {
        const uint32_t sy = dst_y + row;
        for (uint32_t col = 0U; col < width; ++col) {
            const uint32_t sx = dst_x + col;

            if (sx >= image_x && sy >= image_y &&
                (sx - image_x) < image_width &&
                (sy - image_y) < image_height) {
                linebuf[col] = image_pixel_be(image_data,
                                              image_width,
                                              sx - image_x,
                                              sy - image_y);
            } else {
                linebuf[col] = black_be;
            }
        }

        struct display_buffer_descriptor desc = {
            .buf_size = width * sizeof(uint16_t),
            .width = width,
            .height = 1U,
            .pitch = width,
        };

        const int ret = display_write(display, dst_x, sy, &desc, linebuf);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

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
    uint32_t counter = 0U;
    char counter_text[COUNTER_BUF_LEN];

    if (profile == NULL || profile->panel_name == NULL || profile->subtitle == NULL) {
        return -EINVAL;
    }

    const uint32_t fill_rows = (profile->fill_block_rows == 0U) ?
                               DEFAULT_FILL_BLOCK_ROWS :
                               profile->fill_block_rows;

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
    const uint32_t text_area_h = 42U;

    if (profile->image_data != NULL) {
        const uint32_t expected_image_size = profile->image_width * profile->image_height * 2U;
        struct display_buffer_descriptor image_desc = {
            .buf_size = profile->image_buf_size,
            .width = profile->image_width,
            .height = profile->image_height,
            .pitch = profile->image_width,
        };

        if (profile->image_width > width || profile->image_height > height) {
            LOG_ERR("image dimensions exceed panel size");
            return -EINVAL;
        }

        if (profile->image_buf_size < expected_image_size) {
            LOG_ERR("image buffer size mismatch");
            return -EINVAL;
        }

        ret = fill_rect(display, 0U, 0U, width, height, 0x0000, fill_rows);
        if (ret != 0) {
            LOG_ERR("clear screen failed: %d", ret);
            return ret;
        }

        const uint32_t image_x = (width - profile->image_width) / 2U;
        const uint32_t image_y = (height - profile->image_height) / 2U;
        ret = display_write(display, image_x, image_y, &image_desc, profile->image_data);
        if (ret != 0) {
            LOG_ERR("draw image failed: %d", ret);
            return ret;
        }

        const uint32_t text_top = (height > (ZPIX_GLYPH_H * 2U + 6U)) ?
                                  (height - (ZPIX_GLYPH_H * 2U + 6U)) :
                                  0U;
        const uint32_t counter_y = text_top + ZPIX_GLYPH_H + 2U;

        while (1) {
            ret = restore_region_from_image(display,
                                            0U,
                                            text_top,
                                            width,
                                            height - text_top,
                                            profile->image_data,
                                            profile->image_width,
                                            profile->image_height,
                                            image_x,
                                            image_y);
            if (ret != 0) {
                LOG_ERR("restore text background failed: %d", ret);
                return ret;
            }

            ret = draw_text_utf8_image_bg(display,
                                          2U,
                                          text_top,
                                          0x0000,
                                          title_text,
                                          width,
                                          profile->image_data,
                                          profile->image_width,
                                          profile->image_height,
                                          image_x,
                                          image_y);
            if (ret != 0) {
                LOG_ERR("draw image title failed: %d", ret);
                return ret;
            }

            snprintk(counter_text, sizeof(counter_text), "%s%lu", counter_prefix, (unsigned long)counter);
            ret = draw_text_utf8_image_bg(display,
                                          2U,
                                          counter_y,
                                          0x0000,
                                          counter_text,
                                          width,
                                          profile->image_data,
                                          profile->image_width,
                                          profile->image_height,
                                          image_x,
                                          image_y);
            if (ret != 0) {
                LOG_ERR("draw image counter failed: %d", ret);
                return ret;
            }

            ++counter;
            k_msleep(400);
        }
    }

    ret = fill_rect(display, 0U, 0U, width, height, 0x0000, fill_rows);
    if (ret != 0) {
        LOG_ERR("clear screen failed: %d", ret);
        return ret;
    }

    ret = draw_text_utf8(display, 2U, 1U, 0xFFE0, 0x0000, title_text, width);
    if (ret != 0) {
        LOG_ERR("draw title failed: %d", ret);
        return ret;
    }

    ret = draw_text_utf8(display, 2U, 14U, 0x07FF, 0x0000, profile->subtitle, width);
    if (ret != 0) {
        LOG_ERR("draw subtitle failed: %d", ret);
        return ret;
    }

    while (1) {
        const uint32_t bars_y = text_area_h;
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

        ret = fill_rect(display, 0U, 27U, width, 12U, 0x0000, fill_rows);
        if (ret != 0) {
            LOG_ERR("clear counter area failed: %d", ret);
            return ret;
        }

        snprintk(counter_text, sizeof(counter_text), "%s%lu", counter_prefix, (unsigned long)counter);
        ret = draw_text_utf8(display, 2U, 27U, profile->counter_color, 0x0000, counter_text, width);
        if (ret != 0) {
            LOG_ERR("draw counter failed: %d", ret);
            return ret;
        }

        ++counter;
        k_msleep(400);
    }
}