# LVGLV9_Arduino

[English](README.md) · [简体中文](README_ZH.md)

本示例在 720 × 720 MIPI DSI 屏上运行自包含的 LVGL 9 界面，使用 GT911 指针输入
和两个局部 DMA 绘制缓冲区。界面只使用公开的 LVGL 核心控件，因此可直接配合
Arduino Library Manager 软件包使用，不依赖上游源码仓库中独立的 `demos/` 目录。

## 依赖

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- LVGL `9.3.0`
- 仓库内的 `Waveshare_ESP32_P4_4B_Display` 辅助库

相邻的 `lv_conf.h` 是产品资源包针对 LVGL 9.3.0 的配置。修改 LVGL 版本前，
应先审核并迁移该配置。

## 开发板设置

启用 PSRAM，选择 32 MB Flash 和 13 MB 应用分区，并使用
`ChipVariant=prev3`。USB 模式、CDC-on-boot、MSC、DFU 和上传模式保持默认值。

## 运行保护

显示、触摸、LVGL 对象、DMA 缓冲区或 tick 定时器初始化失败时，`setup()` 会停止
并在串口输出第一个错误。传给 LVGL 的绘制缓冲区大小以字节为单位，与 LVGL 9 API
约定一致。

## 硬件边界

触摸使用 SDA GPIO7、SCL GPIO8 和复位 GPIO23。由于参考原理图中触摸中断没有
连接到 ESP32-P4 GPIO，本示例使用轮询。编译不能验证实体板上的显示时序、触摸行为
或内存稳定性。
