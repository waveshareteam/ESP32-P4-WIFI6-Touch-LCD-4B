# AsciiTable

[English](README.md)

在 720 x 720 MIPI DSI 面板上渲染带标签的 ASCII 字符网格。可用于检查基础面板初始化、几何尺寸、颜色和内置字体输出。

## 依赖

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- 仓库内的 `Waveshare_ESP32_P4_4B_Display` 辅助库

## 开发板设置

启用 PSRAM，选择 32 MB Flash 和 13 MB 应用分区，并使用产品示例采用的 `Before v3.00`（`ChipVariant=prev3`）目标。USB 模式、CDC-on-boot、MSC、DFU 和上传模式保持默认值，以使串口输出继续使用开发板的 USB 转 UART 路径。

## 硬件边界

此仅显示草图不使用触摸或 ESP32-C6 无线协处理器。它尚未在本仓库中通过实体开发板验证。
