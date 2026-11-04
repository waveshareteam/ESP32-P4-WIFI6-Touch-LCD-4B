# Camera ISP Tuning

[English](README.md) | [简体中文](README_ZH.md)

## 功能

显示 OV5647 预览，并通过串口监视器调节摄像头 ISP 参数。

## 硬件需求

- ESP32-P4-WIFI6-Touch-LCD-4B 开发板
- OV5647 MIPI-CSI 摄像头模块

## 怎么运行

1. 安装 Arduino-ESP32 3.3.11，选择 ESP32P4 Dev Module。
2. 启用 PSRAM，选择 32 MB Flash 和 Chip Variant v3.00 or newer。
3. 连接摄像头，上传 `07_Camera_ISP_Tuning.ino` 并打开串口监视器。

## 预期现象

显示预览画面，串口命令可调节 ISP 设置。

## 常见问题

上电前请确认 MIPI-CSI 排线方向。
