# img_trans 使用文档（图片转 C 数组）

## 1. 功能说明
`tools/img_trans.c` 用于把图片转换为 RGB565 大端字节流，并输出为 C 源文件数组，适配本仓库显示 demo 的资源格式。

输出文件内容包含：
- `const unsigned char <symbol>[]`
- `const unsigned int <symbol>_len`

## 2. 支持输入格式
仅支持：
- png
- jpeg（包含 jpg 扩展名）

## 3. 剪裁模式
参数 `mode` 支持数字或字符串：

- `0` 或 `center`
  - 中心裁剪
- `1` 或 `top_center`
  - 保留中上区域，必要时裁剪两侧和下方
- `2` 或 `bottom_only`
  - 仅裁剪下部，不裁剪左右
  - 若源图比目标更宽，无法满足目标比例，会报错退出
- `3` 或 `fit_long`
  - 不裁剪，按长边适配
  - 短边补黑边（letterbox）

## 4. 编译工具
在仓库根目录执行：

```bash
mkdir -p tools/bin
cc -O2 -std=c11 tools/img_trans.c -lm -o tools/bin/img_trans
```

## 5. 命令格式
```bash
tools/bin/img_trans <input_image> <output_c> <out_w> <out_h> <mode> [symbol]
```

参数说明：
- `input_image`: 输入图片路径（png/jpg/jpeg）
- `output_c`: 输出 C 文件路径
- `out_w out_h`: 输出分辨率
- `mode`: 剪裁模式
- `symbol`: 可选，输出数组符号名；不填则根据输出文件名自动生成

## 6. 使用示例
### 6.1 生成 240x320（中心裁剪）
```bash
tools/bin/img_trans 2.jpeg src/image_2_240x320_rgb565.c 240 320 center src_image_2_240x320_rgb565
```

### 6.2 生成 128x160（保留中上）
```bash
tools/bin/img_trans 1.jpeg src/image_1_128x160_rgb565.c 128 160 top_center src_image_1_128x160_rgb565
```

### 6.3 生成 128x160（仅裁下部）
```bash
tools/bin/img_trans 1.jpeg src/image_1_128x160_rgb565.c 128 160 bottom_only src_image_1_128x160_rgb565
```

### 6.4 生成 128x160（长边适配，不裁剪）
```bash
tools/bin/img_trans dev.png src/image_dev_128x160_rgb565.c 128 160 fit_long src_image_dev_128x160_rgb565
```

## 7. 常见问题
- 报错 `Unsupported input format. Only png/jpeg are accepted.`
  - 输入文件必须是 png/jpg/jpeg。
- 报错 `mode=bottom_only cannot satisfy target ratio ...`
  - 说明源图相对目标更宽，仅裁下方无法达到目标比例；改用 `center` 或 `top_center`。
- 输出色彩异常
  - 本工具输出 RGB565 大端字节流；请确保显示驱动配置与资源字节序一致。
