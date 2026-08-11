# ST735S Guide for Zephyr 4.4.1

## 0. 引脚澄清（先统一口径）
你这块 1.8 寸 ST7735S 模组是 8 针 SPI 接口：

- GND
- VCC
- SCL
- SDA
- RES
- DC
- CS
- BLK

说明：

- RES 和 RST 在资料中是同义命名（都表示复位引脚）。
- BLK 是背光控制引脚，在 Zephyr 中通常映射为 bl-gpios。

## 1. 目标与背景
这份指南面向你当前的 1.8 寸 SPI 屏资料与 C 驱动，目标是在 Zephyr 4.4.1 下稳定点亮并可维护地集成显示功能。

基于现有代码特征：

- 控制器初始化序列是 ST7735S 风格（常被口语误写成 ST735S）。
- 分辨率为 128x160（或旋转后 160x128）。
- 像素格式为 RGB565（0x3A = 0x05）。
- 地址窗口存在面板偏移补偿（x/y +1/+2）。

## 2. 在 Zephyr 4.4.1 的推荐路线
### 2.1 优先级建议
推荐按以下优先级推进：

1. 优先复用 Zephyr 现有 ST7735R/ST77xx 驱动框架（若参数可覆盖）。
2. 若现有驱动无法覆盖你的偏移和初始化细节，再新增一个轻量自定义驱动。
3. 避免继续使用 bit-bang SPI，直接使用 Zephyr SPI 子系统。

### 2.2 为什么不建议沿用原工程写法
原工程用 GPIO 模拟 SPI，优点是简单，缺点是：

- CPU 占用高
- 时序抖动受中断影响
- 帧率明显受限

在 Zephyr 中，SPI 设备模型 + DMA 更适合 4.4.1 的长期维护。

## 3. 驱动架构建议
### 3.1 分层结构
建议按 Zephyr 驱动分层组织：

- 设备实例层：从 DTS 读取 SPI、DC、RESET、BL 引脚和参数。
- 控制器层：封装命令写、数据写、窗口设置、初始化序列。
- 显示接口层：实现 display_driver_api 的核心回调。

### 3.2 最小需要实现的接口
在 display 子系统中，至少保证：

- blanking_on / blanking_off
- write
- get_capabilities
- set_pixel_format
- set_orientation

若短期只做显示输出，read 可先不实现。

## 4. DTS 设计建议（Zephyr 4.4.1）
### 4.1 节点信息建议
面板节点建议包含：

- SPI 频率
- MADCTL 初值
- x/y 偏移
- reset-gpios
- dc-gpios
- bl-gpios（8 针模组建议配置）
- width / height

### 4.2 关键点
把原工程中的硬编码信息迁移到 DTS/属性，而不是写死在 C 文件：

- USE_HORIZONTAL 对应 orientation 或 MADCTL 策略
- x_offset / y_offset 对应窗口补偿
- 像素格式默认 565

这样更符合 Zephyr 的设备树驱动模式。

## 5. 初始化序列迁移建议
### 5.1 原序列核心
从现有代码提炼的核心命令段：

- 0x11 sleep out
- 0xB1/0xB2/0xB3 frame rate
- 0xC0..0xC5 power/vcom
- 0x36 MADCTL
- 0xE0/0xE1 gamma
- 0x3A 0x05 RGB565
- 0x29 display on

### 5.2 在 Zephyr 中的组织方式
建议把初始化序列组织为表驱动：

- 每项包含 cmd、data、len、delay_ms
- 驱动初始化时按表下发
- 方便替换不同屏厂参数

## 6. 像素写入与性能建议
### 6.1 写入策略
- 小区域刷新增量写
- 全屏刷时尽量使用连续缓冲
- 尽量减少重复发列/行地址命令

### 6.2 推荐性能参数
- SPI 先从中低频起步，逐步升频验证稳定性
- 优先开启 DMA（取决于 SoC SPI 驱动能力）
- 大量刷屏场景优先采用脏矩形策略

## 7. 旋转与偏移的落地建议
这是 ST77xx 移植最常见问题。

建议先固定一组方向，例如竖屏，然后验证：

1. 四角点绘制是否在可见区域
2. 颜色条是否顺序正确
3. 边界是否裁切

若出现整体偏移，再调整 x_offset / y_offset，而不是盲改绘图坐标。

## 8. Kconfig 与工程接入建议
### 8.1 应用侧常见配置思路
建议在 prj.conf 打开：

- 显示子系统
- SPI
- GPIO
- 日志

如果复用 Zephyr 现有 ST77xx 驱动，直接启用对应驱动选项；如果自研驱动，新增自己的 CONFIG 开关并挂到 drivers/display。

### 8.2 调试建议
- 先只做纯色填充测试
- 再做小窗口写入测试
- 最后接入 LVGL 或应用层 UI

## 9. 建议的 bring-up 阶段计划
1. 阶段 1：仅初始化 + 纯色刷屏
2. 阶段 2：窗口写 + 方向切换
3. 阶段 3：字体与图片
4. 阶段 4：性能优化（DMA、分块）

## 10. 常见故障对照
- 黑屏：先查 reset 时序、0x11 后延时、背光引脚。
- 花屏：查 0x3A 与像素字节数是否匹配。
- 偏色：查 RGB/BGR 位与 MADCTL。
- 画面裁切：查 x/y offset 与窗口边界。

---
这块 1.8 寸屏在 Zephyr 4.4.1 上的关键是两件事：

- 用标准 SPI 设备模型替代 bit-bang。
- 把方向与偏移参数化（DTS/Kconfig），避免写死。