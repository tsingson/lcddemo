# Zephyr ESP32 SPI Display Validation

## Environment
- Zephyr target: 4.4.1 (compatible with 4.4.2)
- Board: esp32_devkitc_esp32_procpu
- west path: /Users/qinshen/go/zephyrproject/.venv/bin/west

## Mode Quick Reference

| Goal | APP_DEMO_ENTRY | Overlay (auto) | Extra conf (auto) |
|---|---|---|---|
| ST7735S image switch + running subtitle | st7735s | st7735s | none |
| ST7789 stable slideshow | st7789 | st7789 | prj_st7789.conf |

## End-Of-Day Quick Commands

```bash
# 1) Build ST7735S mode
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DAPP_DEMO_ENTRY=st7735s

# 2) Flash
/Users/qinshen/go/zephyrproject/.venv/bin/west flash --esp-device /dev/cu.usbserial-A5069RR4

# 3) Monitor
/Users/qinshen/go/zephyrproject/.venv/bin/west espressif monitor -p /dev/cu.usbserial-A5069RR4 -b 115200

# 4) Build ST7789 mode
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DAPP_DEMO_ENTRY=st7789
```

## Pin Assignment
ST7735S and ST7789 share the same ESP32 SPI/control pins to minimize rewiring.

### Group A: ST7735S (8-pin)
- GND -> GND
- VCC -> 3V3
- SCL -> GPIO18 (SPI3 SCK)
- SDA -> GPIO23 (SPI3 MOSI)
- RES -> GPIO22
- DC -> GPIO21
- CS -> GPIO5
- BLK -> GPIO4 (optional backlight control)

Overlay file:
- boards/esp32_devkitc_esp32_procpu_st7735s.overlay

### Group B: ST7789 (7-pin)
- GND -> GND
- VCC -> 3V3
- SCL -> GPIO18 (SPI3 SCK)
- SDA -> GPIO23 (SPI3 MOSI)
- RST -> GPIO22
- DC -> GPIO21
- CS -> GPIO5

Overlay file:
- boards/esp32_devkitc_esp32_procpu_st7789.overlay

## Build Commands
From this directory:

### ST7735S demo
```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DAPP_DEMO_ENTRY=st7735s
```

### ST7789 demo
```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DAPP_DEMO_ENTRY=st7789
```

## App Switches In Code
Application-level mode switch is now explicit CMake option:

- APP_DEMO_ENTRY: st7735s | st7789

## Flash And Monitor

```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west flash --esp-device /dev/cu.usbserial-A5069RR4
/Users/qinshen/go/zephyrproject/.venv/bin/west espressif monitor -p /dev/cu.usbserial-A5069RR4 -b 115200
```

## Source Entrypoints
- src/main_st7735s.c
- src/main_st7789.c

## Reusable Display Modules
- src/lcd_demo_common.h / src/lcd_demo_common.c
  - API: int lcd_demo_common_run(const struct lcd_demo_profile *profile)
  - Responsibility: shared UTF-8 rendering, multi-line auto wrap, configurable char/line spacing, image overlay text, color bars, counter update, display_write pipeline
- src/lcd_st7735s.h / src/lcd_st7735s.c
  - API: int lcd_st7735s_demo_run(void)
  - Responsibility: ST7735S profile wrapper and dispatch to common pipeline
- src/lcd_st7789.h / src/lcd_st7789.c
  - API: int lcd_st7789_demo_run(void)
  - Responsibility: ST7789 profile wrapper and dispatch to common pipeline

## Image To C Array
The converter tool is tools/img_trans.c.

- Input formats: png / jpeg (jpg)
- Output: RGB565 big-endian C array (const unsigned char ...[]) and <symbol>_len
- Full usage doc: tools/img_trans_usage.md

Build tool:

```bash
mkdir -p tools/bin
cc -O2 -std=c11 tools/img_trans.c -lm -o tools/bin/img_trans
```

Example:

```bash
tools/bin/img_trans 2.jpeg src/image_2_240x320_rgb565.c 240 320 center src_image_2_240x320_rgb565
```

## Stable Slideshow Tuning
Recommended stable build path:

```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DAPP_DEMO_ENTRY=st7789
```

Explicit in-code tunables:
- src/lcd_st7789.c: ST7789_SWITCH_PERIOD_MS / ST7789_SUBTITLE_FRAME_MS / ST7789_SUBTITLE_STEP_PX / ST7789_STARTUP_WHITE_MS
- src/lcd_st7735s.c: ST7735S_SWITCH_PERIOD_MS / ST7735S_SUBTITLE_FRAME_MS / ST7735S_SUBTITLE_STEP_PX / ST7735S_STARTUP_WHITE_MS
