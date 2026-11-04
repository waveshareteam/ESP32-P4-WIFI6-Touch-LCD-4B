# LCD-4B BSP 兼容层

[English](README.md) | [简体中文](README_ZH.md)

## 功能

保留出厂应用现有的 BSP API，由 ESP Board Manager 管理板级资源。硬件定义来自 [waveshare_boards](https://github.com/waveshareteam/waveshare_boards/tree/v0.3.0/boards/esp32_p4_wifi6_touch_lcd_4b)。

## 显示

托管板定义自动识别 JD9365DA 或 ST7703，并选择相应寄存器序列和 DPI 时序。背光 PWM 使用 GPIO26，GPIO33 保持不配置。

## 音频

兼容 API 借用 Board Manager 的 ES8311/ES7210 编解码器句柄。STD 输出及四槽 TDM 输入保留出厂应用的麦克风和回声参考布局。

## 资源归属

返回的显示、触摸、编解码器和总线句柄均为借用。应用释放自身状态，共享硬件的生命周期由 Board Manager 管理。

## 构建

按[出厂示例](../../README_ZH.md)执行板卡选择和构建。Board Manager 生成代码在本地重建，不纳入源码提交。
