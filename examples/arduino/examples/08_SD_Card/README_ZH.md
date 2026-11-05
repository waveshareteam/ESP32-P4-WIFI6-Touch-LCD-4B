# SD Card

[English](README.md) | [简体中文](README_ZH.md)

## 功能

初始化板载 MicroSD 卡槽并执行文件操作。

## 硬件需求

- ESP32-P4-WIFI6-Touch-LCD-4B 开发板
- MicroSD 卡

## 怎么运行

1. 安装 Arduino-ESP32 3.3.11，选择 ESP32P4 Dev Module。
2. 启用 PSRAM，选择 32 MB Flash 和 Chip Variant v3.00 or newer。
3. 插入已格式化的卡并上传 `08_SD_Card.ino`。

## 预期现象

串口监视器输出卡和文件操作结果。

## 常见问题

请使用正确格式化的 MicroSD 卡。
