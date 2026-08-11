#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app_st7789, LOG_LEVEL_INF);

#if defined(CONFIG_APP_ST7735S_DEMO) && defined(CONFIG_APP_ST7789_DEMO)
#error "Enable only one demo entry: CONFIG_APP_ST7735S_DEMO or CONFIG_APP_ST7789_DEMO"
#endif

#if defined(CONFIG_APP_ST7789_DEMO)

static void fill_line_rgb565(uint16_t *buf, size_t pixels, uint16_t rgb565)
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
    LOG_INF("ST7789 demo: %ux%u", caps.x_resolution, caps.y_resolution);

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
    static uint16_t linebuf[320];

    if (width > ARRAY_SIZE(linebuf)) {
        LOG_ERR("line buffer too small for width=%u", width);
        return -EINVAL;
    }

    desc.buf_size = width * sizeof(uint16_t);
    desc.width = width;
    desc.height = 1U;
    desc.pitch = width;

    const uint16_t bars[] = {
        0xF800, /* red */
        0x07E0, /* green */
        0x001F, /* blue */
        0xFFE0, /* yellow */
        0xF81F, /* magenta */
        0x07FF, /* cyan */
        0xFFFF, /* white */
        0x0000, /* black */
    };

    const uint32_t block = (caps.y_resolution / ARRAY_SIZE(bars)) == 0U ? 1U :
                           (caps.y_resolution / ARRAY_SIZE(bars));

    while (1) {
        for (size_t i = 0; i < ARRAY_SIZE(bars); ++i) {
            fill_line_rgb565(linebuf, width, bars[i]);

            const uint32_t y_start = (uint32_t)i * block;
            const uint32_t y_end = (i == ARRAY_SIZE(bars) - 1U) ?
                                   caps.y_resolution :
                                   ((uint32_t)(i + 1U) * block);

            for (uint32_t y = y_start; y < y_end; ++y) {
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
