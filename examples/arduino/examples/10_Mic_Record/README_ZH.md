# Microphone Record

[English](README.md) | [简体中文](README_ZH.md)

## 功能

采集 ES7210 麦克风音频，并通过串口输出采样数据。

## 硬件需求

- ESP32-P4-WIFI6-Touch-LCD-4B 开发板

## 怎么运行

1. 安装 Arduino-ESP32 3.3.11，选择 ESP32P4 Dev Module。
2. 启用 PSRAM，选择 32 MB Flash 和 Chip Variant v3.00 or newer。
3. 上传 `10_Mic_Record.ino` 并打开串口监视器。

## 预期现象

串口监视器输出采集的音频数值。

## 常见问题

请使用草图中配置的串口波特率。
