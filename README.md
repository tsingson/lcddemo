# Zephyr ESP32 SPI Display Validation

## Environment
- Zephyr target: 4.4.1 (compatible with 4.4.2)
- Board: esp32_devkitc_esp32_procpu
- west path: /Users/qinshen/go/zephyrproject/.venv/bin/west

## Pin Assignment (Two Non-Conflicting Groups)
Both displays share SPI2 clock/data lines, but each display uses its own CS/DC/RST control pins, so wiring groups do not conflict.

### Group A: ST7735S (8-pin)
- GND -> GND
- VCC -> 3V3
- SCL -> GPIO18 (SPI2 SCK)
- SDA -> GPIO23 (SPI2 MOSI)
- RES -> GPIO22
- DC -> GPIO21
- CS -> GPIO5
- BLK -> GPIO4 (optional backlight control)

Overlay file:
- boards/esp32_devkitc_esp32_procpu_st7735s.overlay

### Group B: ST7789 (7-pin)
- GND -> GND
- VCC -> 3V3
- SCL -> GPIO18 (SPI2 SCK)
- SDA -> GPIO23 (SPI2 MOSI)
- RST -> GPIO26
- DC -> GPIO27
- CS -> GPIO15

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

Only one demo entry should be enabled at a time by Kconfig.
