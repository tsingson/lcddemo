#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app_st7735s, LOG_LEVEL_INF);

#if defined(CONFIG_APP_ST7735S_DEMO) && defined(CONFIG_APP_ST7789_DEMO)
#error "Enable only one demo entry: CONFIG_APP_ST7735S_DEMO or CONFIG_APP_ST7789_DEMO"
#endif

#if defined(CONFIG_APP_ST7735S_DEMO)

#define BAR_HEIGHT 32U

static void fill_color_bar(uint16_t *buf, size_t pixels, uint16_t rgb565)
{
    const uint16_t be = sys_cpu_to_be16(rgb565);

    for (size_t i = 0; i < pixels; ++i) {
        buf[i] = be;
    }
}

int main(void)
{
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    struct display_capabilities caps;
    struct display_buffer_descriptor desc;

    if (!device_is_ready(display)) {
        LOG_ERR("Display device is not ready");
        return -ENODEV;
    }

    display_get_capabilities(display, &caps);
    LOG_INF("ST7735S demo: %ux%u", caps.x_resolution, caps.y_resolution);

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
    const uint32_t max_rows = (caps.y_resolution < BAR_HEIGHT) ? caps.y_resolution : BAR_HEIGHT;
    static uint16_t linebuf[320];

    if (width > ARRAY_SIZE(linebuf)) {
        LOG_ERR("line buffer too small for width=%u", width);
        return -EINVAL;
    }

    desc.buf_size = width * sizeof(uint16_t);
    desc.width = width;
    desc.height = 1U;
    desc.pitch = width;

    const uint16_t colors[] = {
        0xF800, /* red */
        0x07E0, /* green */
        0x001F, /* blue */
        0xFFFF, /* white */
        0x0000, /* black */
    };

    while (1) {
        for (size_t c = 0; c < ARRAY_SIZE(colors); ++c) {
            fill_color_bar(linebuf, width, colors[c]);

            const uint32_t y0 = (uint32_t)c * max_rows;
            for (uint32_t y = y0; y < (y0 + max_rows) && y < caps.y_resolution; ++y) {
                ret = display_write(display, 0U, y, &desc, linebuf);
                if (ret != 0) {
                    LOG_ERR("display_write failed at y=%u, ret=%d", y, ret);
                    return ret;
                }
            }
        }

        k_msleep(1200);
    }
}

#endif
