# HelloWorld

[English](README.md)

初始化 720 x 720 MIPI DSI 面板，打印问候语，然后以随机位置、颜色和大小绘制相同文本。

## 依赖

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- 仓库内的 `Waveshare_ESP32_P4_4B_Display` 辅助库

## 开发板设置

启用 PSRAM，选择 32 MB Flash 和 13 MB 应用分区，并使用 `ChipVariant=prev3`。USB 模式、CDC-on-boot、MSC、DFU 和上传模式保持默认值。

## 硬件边界

此仅显示草图不使用触摸或 ESP32-C6 无线协处理器。它尚未在本仓库中通过实体开发板验证。
