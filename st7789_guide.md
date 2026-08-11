# ST7789 Guide for Zephyr 4.4.1

## 0. 引脚澄清（先统一口径）
你这块 2.0 寸 ST7789 模组是 7 针 SPI 接口：

- GND
- VCC
- SCL
- SDA
- RST
- DC
- CS

说明：

- 这套 7 针定义里没有独立 BLK 背光脚。
- 如果你的实际硬件板额外引出了背光控制脚，再在 Zephyr DTS 中增加 bl-gpios。

当前工程默认接线（与 ST7735S 尽量一致）：

- SCL -> GPIO18（SPI3 SCK）
- SDA -> GPIO23（SPI3 MOSI）
- CS -> GPIO5
- DC -> GPIO21
- RST -> GPIO22

## 1. 目标与资料结论
这份指南对应你当前 2.0 寸 TFT 资料，面向 Zephyr 4.4.1 的驱动落地。

已知资料特征：

- 分辨率 240x320
- 控制器为 ST7789 系列（代码注释写 ST7789V2）
- 参考代码使用软件 SPI
- 参考代码采用 RGB666（0x3A = 0x66，3 字节像素）
- 两份初始化文本采用 RGB565（0x3A = 0x05，2 字节像素）

核心风险：初始化参数和像素格式存在多版本差异，必须先统一策略。

## 2. Zephyr 4.4.1 的实现路线建议
### 2.1 推荐路线
1. 优先评估 Zephyr 现有 ST7789 驱动是否可直接复用。
2. 若无法覆盖你面板的偏移、gamma、供电参数，再派生一个专用驱动。
3. 使用 SPI 子系统，不建议保留 bit-bang。

### 2.2 面板参数策略
先定一个“唯一真值版本”：

- 如果用 16bit 资源链路（图像/字体都 565），选 0x3A=0x05。
- 如果坚持 18bit 显示链路，选 0x3A=0x66，并保证全链路 3 字节像素。

不要把两份初始化表混搭。

## 3. 驱动架构建议
### 3.1 建议分层
- 总线层：SPI 发送命令/数据
- 控制器层：reset、init、set_window
- 显示层：实现 display_driver_api

### 3.2 初始化建议
采用表驱动初始化结构：

- cmd
- payload 指针
- payload 长度
- post_delay_ms

这样便于按不同面板 SKU 切换参数。

## 4. DTS/板级建模建议
### 4.1 建议放入设备树的参数
- spi-max-frequency
- reset-gpios
- dc-gpios
- bl-gpios（仅在硬件额外引出背光控制时配置）
- width / height
- x_offset / y_offset
- madctl
- pixel-format

### 4.2 偏移处理
你的参考代码中行地址起始有 0x28 偏移，这类信息应作为面板参数保存，而不是硬编码在 write 路径里。

## 5. 像素格式与资源链路建议
### 5.1 推荐默认方案
对 Zephyr 应用生态（LVGL、图片资源、内存占用）而言，建议优先 565：

- 0x3A = 0x05
- 每像素 2 字节
- 显存带宽和 RAM 压力更可控

### 5.2 何时考虑 666
仅在你明确需要更高色深且 SoC/SPI 带宽充足时，再使用 666。

## 6. 性能建议（240x320 重点）
### 6.1 带宽现实
- 565：153600 字节/帧
- 666：230400 字节/帧

240x320 下软件 SPI 性能通常不足以支撑流畅大面积刷新。

### 6.2 优化建议
- 必须切到硬件 SPI
- 尽量启用 DMA
- 分块传输，控制单次缓冲大小
- 通过脏矩形减少全屏刷新

## 7. 初始化参数选择建议
你目录中有两份参数表（HSD 和 BOE 版本），建议流程：

1. 确认当前实物模组供应商和玻璃型号。
2. 选定对应参数表作为基线。
3. 只做小幅调优（gamma、反相、方向），不跨表拼参数。

常见症状映射：

- 偏色/灰阶断层：优先查 E0/E1 gamma
- 闪烁/亮度异常：查 BB/C0/C3/C4/D0
- 画面偏移：查 2A/2B 起始与 offset

## 8. Zephyr 4.4.1 接入建议
### 8.1 工程配置思路
在应用配置中启用：

- display
- spi
- gpio
- log

若复用官方驱动，直接开启对应驱动选项；若自定义驱动，建议按 Zephyr display 驱动目录结构新增，并提供 Kconfig 与 CMakeLists 集成。

### 8.2 bring-up 测试顺序
1. reset + sleep out + display on
2. 全屏纯色（红绿蓝白黑）
3. 局部窗口填充
4. 方向切换
5. 字库/图片
6. 应用层 UI

## 9. 建议的代码组织
建议将当前单文件参考代码拆分为：

- st7789_bus.c: SPI 读写基础
- st7789_panel.c: init table 和窗口控制
- st7789_display.c: Zephyr display API 对接
- st7789_panel_profiles.h: HSD/BOE 参数集

好处：

- 面板参数可管理
- 总线可替换
- 应用层无感知

## 10. 常见问题速查
- 黑屏：reset 时序、0x11 延时、背光 GPIO。
- 花屏：0x3A 和像素字节数不匹配。
- 上下裁切：2B 起始/结束地址或 y_offset 不匹配。
- 颜色反转：0x21/0x20 与 MADCTL 组合不匹配。

## 11. 当前工程中的 ST7789 驱动拆分
为了提升复用性，工程采用“公共层 + 面板配置层”结构。

公共层：

- src/lcd_demo_common.h
- src/lcd_demo_common.c

该层统一提供：

- UTF-8 解码
- zpix12 字库查找与绘制
- 彩条与计数刷新主循环
- 分块 display_write 刷新策略

ST7789 面板层：

- src/lcd_st7789.h
- src/lcd_st7789.c

接口说明：

- int lcd_st7789_demo_run(void)

职责边界：

- src/main_st7789.c 只保留入口分发。
- src/lcd_st7789.c 仅维护 ST7789 profile（面板名、副标题、计数颜色、分块行数），并调用公共层入口。
- src/zpix12_font_data.c/.h 作为通用字库数据源，由 src/lcd_demo_common.c 统一调用。

复用建议：

- 新应用需要快速点屏时，可直接调用 lcd_st7789_demo_run。
- 若需要更强可组合性，优先在 src/lcd_demo_common.c 扩展通用接口，再由各面板 profile 复用。
- 不同屏幕供应商参数切换时，优先保持入口文件不变，只在 lcd_st7789.c 内部策略与参数处调整。

---
在 Zephyr 4.4.1 上，这块 2.0 寸 ST7789 的成功关键是：

- 先统一参数版本（尤其 565/666 策略）。
- 走标准 SPI + display 驱动路线，而不是继续 bit-bang。