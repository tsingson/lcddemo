# Zephyr ESP32 SPI Display Validation

## Environment
- Zephyr target: 4.4.1 (compatible with 4.4.2)
- Board: esp32_devkitc_esp32_procpu
- west path: /Users/qinshen/go/zephyrproject/.venv/bin/west

## Pin Assignment
ST7735S and ST7789 can now share the same ESP32 SPI/control pins to minimize rewiring.

### Group A: ST7735S (8-pin)
- GND -> GND
- VCC -> 3V3
- SCL -> GPIO18 (SPI3 SCK)
- SDA -> GPIO23 (SPI3 MOSI)
- RES -> GPIO22
- DC -> GPIO21
- CS -> GPIO5
- BLK -> GPIO4 (optional backlight control)

Note: On esp32_devkitc/esp32/procpu, SPI3 defaults are GPIO18/GPIO23/GPIO5.

Overlay file:
- boards/esp32_devkitc_esp32_procpu_st7735s.overlay

### Group B: ST7789 (7-pin)
- GND -> GND
- VCC -> 3V3
- SCL -> GPIO18 (SPI3 SCK, same as ST7735S)
- SDA -> GPIO23 (SPI3 MOSI, same as ST7735S)
- RST -> GPIO22 (same as ST7735S)
- DC -> GPIO21 (same as ST7735S)
- CS -> GPIO5 (same as ST7735S)

Overlay file:
- boards/esp32_devkitc_esp32_procpu_st7789.overlay

## Build Commands
From this directory:

### ST7735S demo
```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7735s.overlay
```

### ST7789 demo
```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7789.overlay -DEXTRA_CONF_FILE=prj_st7789.conf
```

## Source Entrypoints
- src/main_st7735s.c
- src/main_st7789.c

## Reusable Display Modules
- src/lcd_demo_common.h / src/lcd_demo_common.c
	- API: int lcd_demo_common_run(const struct lcd_demo_profile *profile)
	- Responsibility: shared UTF-8/font rendering, color bars, counter update, display write pipeline
- src/lcd_st7735s.h / src/lcd_st7735s.c
	- API: int lcd_st7735s_demo_run(void)
	- Responsibility: ST7735S profile wrapper (panel name/subtitle/color/fill rows) and dispatch to common pipeline
- src/lcd_st7789.h / src/lcd_st7789.c
	- API: int lcd_st7789_demo_run(void)
	- Responsibility: ST7789 profile wrapper (panel name/subtitle/color/fill rows) and dispatch to common pipeline

Main files now only dispatch to reusable modules, making later feature reuse easier.

Only one demo entry should be enabled at a time by Kconfig.
