[English](README.md) · [Arduino 示例](../../README_ZH.md)

# LCD-4B Arduino 显示辅助库

此首方辅助库提供随附 Arduino 草图使用的 LCD-4B 板级层：720 x 720 ST7703 双通道
MIPI-DSI 面板、GPIO27 LCD 复位、GPIO26 PWM 背光与 GPIO33 使能，以及 GPIO7/GPIO8
共享 I2C 总线。

GT911 辅助库刻意不驱动触摸 INT 或 RST。它先探测 I2C 地址 `0x5D`，再探测 `0x14`，
使用响应的地址并以轮询方式读取触摸状态。编译成功不代表已在硬件上验证触摸行为。

同级的 `GFX_Library_for_Arduino` 与 `lvgl` 目录是完整随附的第三方库，通过 Arduino
`--libraries` 选项使用，并非独立的产品草图。
