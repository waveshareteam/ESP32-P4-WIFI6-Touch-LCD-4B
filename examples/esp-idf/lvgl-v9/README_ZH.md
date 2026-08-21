# LVGL 9 基准测试

[English](README.md)

通过 `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.1 和 `espressif/esp_lvgl_adapter` 0.6.3，在 4 英寸 720 x 720 面板上运行 LVGL 9.3.0 基准测试。BSP 显示栈启动后，背光亮度设为 50%。

BSP 管理显示刷新、触摸注册和 LVGL 任务。创建应用程序 UI 时需要持有 BSP 显示锁。

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

仓库 CI 面向 ESP-IDF v5.5.5 和 v6.0.2。基准分数、撕裂、触摸行为、热特性和内存余量必须在硬件上测量。
