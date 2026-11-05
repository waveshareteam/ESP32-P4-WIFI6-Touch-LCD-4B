# Drawing Board

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用 GT911 触摸控制器在显示屏上绘制触摸轨迹。

## 硬件需求

- ESP32-P4-WIFI6-Touch-LCD-4B 开发板

## 怎么运行

1. 安装 Arduino-ESP32 3.3.11，选择 ESP32P4 Dev Module。
2. 启用 PSRAM，选择 32 MB Flash 和 Chip Variant v3.00 or newer。
3. 上传 `03_Drawing_board.ino`。

## 预期现象

触摸输入会在屏幕上绘制线条。

## 常见问题

驱动会探测 GT911 的 `0x5D` 和 `0x14` I2C 地址。
