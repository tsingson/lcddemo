# Zephyr ESP32 SPI Display Validation

## Environment
- Zephyr target: 4.4.1 (compatible with 4.4.2)
- Board: esp32_devkitc_esp32_procpu
- west path: /Users/qinshen/go/zephyrproject/.venv/bin/west

## End-Of-Day Quick Commands

```bash
# 1) Build ST7735S stable
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7735s.overlay -DAPP_DEMO_ENTRY=st7735s -DAPP_USE_LVGL_DEMO=OFF

# 2) Flash
/Users/qinshen/go/zephyrproject/.venv/bin/west flash --esp-device /dev/cu.usbserial-A5069RR4

# 3) Monitor
/Users/qinshen/go/zephyrproject/.venv/bin/west espressif monitor -p /dev/cu.usbserial-A5069RR4 -b 115200

# 4) Image -> C array
mkdir -p tools/bin
cc -O2 -std=c11 tools/img_trans.c -lm -o tools/bin/img_trans
tools/bin/img_trans 2.jpeg src/image_2_240x320_rgb565.c 240 320 center src_image_2_240x320_rgb565
```

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
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7735s.overlay -DAPP_DEMO_ENTRY=st7735s
```

### ST7789 demo
```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7789.overlay -DEXTRA_CONF_FILE=prj_st7789.conf -DAPP_DEMO_ENTRY=st7789
```

### ST7789 demo (LVGL path)
```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7789.overlay -DEXTRA_CONF_FILE="prj_st7789.conf;prj_lvgl.conf" -DAPP_DEMO_ENTRY=st7789_lvgl_debug -DAPP_USE_LVGL_DEMO=ON
```

## Zephyr Build Matrix

Template:

```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- <cmake_defines>
```

Command matrix:

```bash
# ST7735S stable demo
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7735s.overlay -DAPP_DEMO_ENTRY=st7735s -DAPP_USE_LVGL_DEMO=OFF

# ST7789 stable demo
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7789.overlay -DEXTRA_CONF_FILE=prj_st7789.conf -DAPP_DEMO_ENTRY=st7789 -DAPP_USE_LVGL_DEMO=OFF

# ST7789 LVGL debug demo
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7789.overlay -DEXTRA_CONF_FILE="prj_st7789.conf;prj_lvgl.conf" -DAPP_DEMO_ENTRY=st7789_lvgl_debug -DAPP_USE_LVGL_DEMO=ON
```

Flash and monitor:

```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west flash --esp-device /dev/cu.usbserial-A5069RR4
/Users/qinshen/go/zephyrproject/.venv/bin/west espressif monitor -p /dev/cu.usbserial-A5069RR4 -b 115200
```

Notes:

- `west espressif monitor` must use `-p` for serial port.
- `--esp-device` is for `west flash`.
- Keep `-p always` when overlay or prj conf changes.

## Image To C Array

The converter tool is `tools/img_trans.c`.

- Input formats: png / jpeg (jpg)
- Output: RGB565 big-endian C array (`const unsigned char ...[]`) and `<symbol>_len`
- Full usage doc: `tools/img_trans_usage.md`

Build tool:

```bash
mkdir -p tools/bin
cc -O2 -std=c11 tools/img_trans.c -lm -o tools/bin/img_trans
```

Example:

```bash
tools/bin/img_trans 2.jpeg src/image_2_240x320_rgb565.c 240 320 center src_image_2_240x320_rgb565
```

## App Switches In Code (Not Kconfig)

Application-level switches have been moved out of Kconfig and are now explicit CMake/code options:

- `APP_DEMO_ENTRY`: `st7735s` | `st7789` | `st7789_lvgl_debug`
- `APP_USE_LVGL_DEMO`: `ON` | `OFF`

Notes:

- These two options control only app logic and entry selection.
- Zephyr subsystem options (for example `CONFIG_LVGL`) still belong to `prj*.conf`.

## Flash And Monitor

```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west flash --esp-device /dev/cu.usbserial-A5069RR4
/Users/qinshen/go/zephyrproject/.venv/bin/west espressif monitor -p /dev/cu.usbserial-A5069RR4 -b 115200
```

Note: `west espressif monitor` uses `-p` for serial port. `--esp-device` is for flash.

## LVGL Usage (Detailed)

### 1) Purpose and scope

The LVGL path is used only for ST7789 runtime verification and debugging, isolated in `src/main_st7789_lgvl.c`.

### 2) Build requirements

- Enable Zephyr LVGL stack through `prj_lvgl.conf` (`CONFIG_LVGL=y` and related memory settings).
- Select app entry by CMake: `-DAPP_DEMO_ENTRY=st7789_lvgl_debug`.
- Enable app LVGL branch by CMake: `-DAPP_USE_LVGL_DEMO=ON`.

### 3) Runtime flow and API handling

Display/device phase (shared in `lcd_demo_common_run`):

1. `DEVICE_DT_GET(DT_CHOSEN(zephyr_display))`
2. `device_is_ready()`
3. `display_get_capabilities()`
4. `display_set_pixel_format(PIXEL_FORMAT_RGB_565)`
5. `display_blanking_off()`

LVGL phase (in `run_lvgl_demo`):

1. `lv_display_get_default()` checks LVGL display binding.
2. `lv_scr_act()` gets active screen object.
3. `lv_obj_set_style_bg_color/opa()` sets screen background.
4. `lv_label_create()` creates title and counter labels.
5. `lv_obj_create()` creates heartbeat indicator.
6. `lv_timer_create()` sets periodic counter/heartbeat updates.
7. `lv_refr_now(NULL)` forces immediate first refresh.
8. Main loop calls `lv_timer_handler()` + `k_msleep(20)`.

Timer callback handling (`lvgl_counter_timer_cb`):

- Uses `lv_timer_get_user_data(timer)` to read context safely.
- Updates label text with `lv_label_set_text_fmt()`.
- Toggles heartbeat dot color via `lv_obj_set_style_bg_color()`.
- Emits periodic `LOG_INF` heartbeat for diagnosis.

### 4) Error handling policy

- Missing display device: return `-ENODEV`.
- LVGL default display or active screen missing: return `-ENODEV`.
- LVGL timer creation failure: return `-ENOMEM`.
- Display API failures: return original error code and log detail.

### 5) Practical tuning points

- Timer period: `500 ms` in `lv_timer_create`.
- LVGL loop period: `20 ms` in main loop.
- LVGL memory/stack: configured in `prj_lvgl.conf`.

## Source Entrypoints
- src/main_st7735s.c
- src/main_st7789.c
- src/main_st7789_lgvl.c

## Reusable Display Modules
- src/lcd_demo_common.h / src/lcd_demo_common.c
	- API: int lcd_demo_common_run(const struct lcd_demo_profile *profile)
	- Responsibility: shared UTF-8 rendering, multi-line auto wrap, configurable char/line spacing, image overlay text, color bars, counter update, display_write pipeline
- src/lcd_st7735s.h / src/lcd_st7735s.c
	- API: int lcd_st7735s_demo_run(void)
	- Responsibility: ST7735S profile wrapper (title/subtitle/counter style and spacing) and dispatch to common pipeline
- src/lcd_st7789.h / src/lcd_st7789.c
	- API: int lcd_st7789_demo_run(void)
	- Responsibility: ST7789 profile wrapper (image mode, wrapped text style/spacing) and dispatch to common pipeline

## Text Layout Configuration
The profile struct now supports text-layout tuning:

- title / subtitle / counter_prefix
- title_color / subtitle_color / counter_color / bg_color
- char_spacing (glyph spacing)
- line_spacing (line spacing)
- counter_period_ms

Both Chinese and English strings are rendered with UTF-8 decode and automatic wrapping.

## Zephyr 4.4.2 API Conventions
Display path follows Zephyr display conventions:

- DEVICE_DT_GET(DT_CHOSEN(zephyr_display)) for device binding
- display_get_capabilities for runtime panel dimensions
- display_set_pixel_format(PIXEL_FORMAT_RGB_565)
- display_blanking_off before frame updates
- display_write with explicit display_buffer_descriptor

All large-area writes use chunked transfer to keep memory bounded and improve reliability.

## Overlay Notes And Cautions

General overlay rules:

- Keep `zephyr,display = &panel_node` aligned with the selected panel node.
- Keep SPI bus and control pins consistent with wiring (SCK/MOSI/CS/DC/RESET).
- After any overlay change, rebuild with `-p always` to avoid stale devicetree artifacts.

ST7735S overlay (`boards/esp32_devkitc_esp32_procpu_st7735s.overlay`):

- Keep `width/height/x-offset/y-offset` aligned to 128x160 panel glass.
- Keep inversion disabled on this panel; enabling inversion causes negative colors.
- If color channels swap (red/blue), adjust `madctl` RGB/BGR bit only.

ST7789 overlay (`boards/esp32_devkitc_esp32_procpu_st7789.overlay`):

- Keep `colmod = <0x55>` for RGB565 path used by this app.
- Validate `mdac`, gamma, porch and RAM params as one coherent set; avoid cross-mixing parameter tables.
- If image shifts or crops, recheck `x-offset/y-offset` first.

## Stable Slideshow Tuning
Recommended stable build path (non-LVGL):

```bash
/Users/qinshen/go/zephyrproject/.venv/bin/west build -p always -b esp32_devkitc/esp32/procpu . -- -DDTC_OVERLAY_FILE=boards/esp32_devkitc_esp32_procpu_st7789.overlay -DEXTRA_CONF_FILE=prj_st7789.conf -DAPP_DEMO_ENTRY=st7789 -DAPP_USE_LVGL_DEMO=OFF
```

Explicit in-code tunables for long-term stabilization:

- src/lcd_st7789.c: ST7789_SWITCH_PERIOD_MS / ST7789_SUBTITLE_FRAME_MS / ST7789_SUBTITLE_STEP_PX / ST7789_STARTUP_WHITE_MS
- src/lcd_st7735s.c: ST7735S_SWITCH_PERIOD_MS / ST7735S_SUBTITLE_FRAME_MS / ST7735S_SUBTITLE_STEP_PX / ST7735S_STARTUP_WHITE_MS

## ST7735S Color Hardening Notes

- The stable ST7735S overlay keeps `inversion-on` disabled to avoid negative colors.
- If color is still wrong but animation is correct, adjust `madctl` (RGB/BGR bit) in `boards/esp32_devkitc_esp32_procpu_st7735s.overlay` for one-step A/B verification.
