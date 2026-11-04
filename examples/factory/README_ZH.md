# 出厂固件

[English](README.md) | [简体中文](README_ZH.md)

## 功能

构建 ESP32-P4-WIFI6-Touch-LCD-4B 的出厂应用。应用会识别连接的 MIPI-DSI
显示控制器，并初始化匹配的 ST7703 或 JD9365 面板序列。

## 硬件需求

- ESP32-P4-WIFI6-Touch-LCD-4B 开发板

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

使用 ESP-IDF v5.5.5。串口启动日志会标识所选面板。

## 预期现象

应用启动产品 UI，并使用匹配的显示配置。

## 常见问题

请使用出厂固件指定的 ESP-IDF 版本；其他版本不在其构建矩阵中。
