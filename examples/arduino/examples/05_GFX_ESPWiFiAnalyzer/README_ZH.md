# Wi-Fi Analyzer

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 ESP32-C6 协处理器扫描 Wi-Fi 网络并在显示屏显示结果。

## 硬件需求

- ESP32-P4-WIFI6-Touch-LCD-4B 开发板

## 怎么运行

1. 安装 Arduino-ESP32 3.3.11，选择 ESP32P4 Dev Module。
2. 启用 PSRAM，选择 32 MB Flash 和 Chip Variant v3.00 or newer。
3. 上传 `05_GFX_ESPWiFiAnalyzer.ino`。

## 预期现象

显示屏列出检测到的 Wi-Fi 网络。

## 常见问题

ESP32-C6 需要兼容的 Hosted 固件。
