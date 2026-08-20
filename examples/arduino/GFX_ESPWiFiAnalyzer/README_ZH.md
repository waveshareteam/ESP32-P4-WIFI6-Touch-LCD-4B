# GFX_ESPWiFiAnalyzer

[English](README.md)

扫描附近的 2.4 GHz Wi-Fi 网络，并在 720 x 720 显示屏上绘制信道占用和信号强度。

## 依赖

- Arduino-ESP32 `3.3.11`，包括其 `WiFi` 和 `Network` 库
- GFX Library for Arduino `1.6.6`
- 仓库内的 `Waveshare_ESP32_P4_4B_Display` 辅助库

## 开发板设置

启用 PSRAM，选择 32 MB Flash 和 13 MB 应用分区，并使用 `ChipVariant=postv3`。USB 模式、CDC-on-boot、MSC、DFU 和上传模式保持默认值。

## 无线边界

ESP32-P4 没有集成 Wi-Fi 无线电。运行时 Wi-Fi 依赖板载 ESP32-C6 运行与 Arduino-ESP32 主机栈兼容的固件。主机构建成功不能证明 C6 协议和固件匹配。本草图尚未在本仓库中通过实体开发板验证。
