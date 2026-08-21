# LVGL 8 控件

[English](README.md)

通过 `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.1 和 `espressif/esp_lvgl_adapter` 0.6.3，在 4 英寸 720 x 720 面板上运行 LVGL 8.4.0 控件演示。

BSP 启动显示、触摸输入、LVGL 适配器和 LVGL 任务。创建应用程序 UI 时需要持有 BSP 显示锁。

BSP 3.0.1 在其显示 API 中使用 LVGL 9 的旋转类型拼写。该项目为 LVGL 8 的等效类型应用仅用于编译的别名；不会派生或修改托管 BSP。

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

仓库 CI 面向 ESP-IDF v5.5.5 和 v6.0.2。视觉输出、触摸方向、撕裂、帧率和内存余量仍属于硬件验证项目。
