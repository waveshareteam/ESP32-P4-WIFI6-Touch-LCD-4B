# LVGL v9 Arduino

[English](README.md) | [简体中文](README_ZH.md)

## 功能

运行带 GT911 触摸输入的 LVGL v9 控件界面。

## 硬件需求

- ESP32-P4-WIFI6-Touch-LCD-4B 开发板

## 怎么运行

1. 安装 Arduino-ESP32 3.3.11，选择 ESP32P4 Dev Module。
2. 启用 PSRAM，选择 32 MB Flash 和 Chip Variant v3.00 or newer。
3. 上传 `04_LVGLV9_Arduino.ino`。

## 预期现象

LVGL 界面响应触摸操作。

## 常见问题

驱动会探测 GT911 的 `0x5D` 和 `0x14` I2C 地址。
