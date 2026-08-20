# Drawing_board

[English](README.md)

轮询 GT911 触摸控制器，并在 720 x 720 MIPI DSI 面板上绘制蓝点，提供最小化的显示与触摸交互检查。

## 依赖

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- 仓库内的 `Waveshare_ESP32_P4_4B_Display` 辅助库

## 开发板设置

启用 PSRAM，选择 32 MB Flash 和 13 MB 应用分区，并使用 `ChipVariant=postv3`。USB 模式、CDC-on-boot、MSC、DFU 和上传模式保持默认值。

## 硬件边界

触摸使用 SDA GPIO7 和 SCL GPIO8。辅助库不驱动 GT911 的 INT 或 RST：先探测 `0x5D`，再探测
`0x14`，以响应的地址初始化。参考原理图中触摸中断未连接到 ESP32-P4 GPIO，因此本草图轮询
控制器。它尚未在本仓库中通过实体开发板验证。
