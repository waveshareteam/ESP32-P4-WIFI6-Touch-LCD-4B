# ST7703 LCD 色彩测试

[English](README.md)

面向 4 英寸 ST7703 面板的低级 720 x 720 MIPI DSI 测试。它通过片上 LDO 通道 3 为 DSI PHY 供电，在 GPIO 27 上复位面板，在 GPIO 26 上控制背光，并显示控制器的垂直彩条图案。

该项目直接使用 `waveshare/esp_lcd_st7703` 2.0.0，而不使用 LVGL 或开发板 BSP。当需要隔离面板、时序、复位或背光问题时，此项目很有用。

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

仓库 CI 面向 ESP-IDF v5.5.5 和 v6.0.2。编译成功无法确认 DSI 信号完整性、面板版本、颜色顺序或背光极性。
