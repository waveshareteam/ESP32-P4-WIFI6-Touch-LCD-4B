# Waveshare ESP32-P4 WIFI6 Touch LCD 4B 显示支持

[English](README.md)

这是从官方 ESP32-P4-WIFI6-Touch-LCD-4B 示例归档包导入并由本仓库维护的开发板专用
Arduino 辅助库。它包含 720 x 720 ST7703 面板初始化与时序数据，以及本仓库首方草图
共用的 I2C 和 GT911 触摸实现。

## 硬件映射

- MIPI DSI 面板：720 x 720、2 通道、每通道 480 Mbps
- LCD 复位：GPIO27
- 触摸控制器：GT911；辅助库先探测 I2C 地址 `0x5D`，再探测 `0x14`，并以响应地址初始化
- 触摸 I2C：SDA GPIO7、SCL GPIO8
- 触摸复位和中断：辅助库不驱动；参考开发板原理图中的中断未连接到 ESP32-P4 GPIO

`displays_config.h` 中的时序和初始化序列保留自官方产品包。导入辅助库时，仅将显示名称
规范为产品名称。

由于未配置中断 GPIO，GT911 输入通过轮询读取。地址探测与轮询路径仅描述软件行为；
物理地址选择时序和触摸操作仍需硬件验证。

## Arduino 依赖

仓库 CI 固定以下版本：

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- 仅 LVGL 草图使用 LVGL `9.3.0`

官方产品归档包包含 GFX Library for Arduino `1.6.0` 与 LVGL `9.3.0`。此处使用 GFX
`1.6.6` 以兼容固定的当前 Arduino-ESP32 core。辅助库本身不含 LVGL 代码。

## 来源与许可边界

- 产品文档：https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B
- 官方示例归档包：https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/ESP32-P4-WIFI6-Touch-LCD-4B.zip

导入的 `gt911.h` 和 `gt911.cpp` 文件带有各自的 Apache-2.0 SPDX 声明。辅助库其余文件
在官方归档包中未声明许可证。本仓库不会推断或授予这些文件的许可证；请保留所有文件级
声明，并向 Waveshare 咨询相关条款。
