# ST7703 LCD color test

[中文](README_ZH.md)

Low-level 720 x 720 MIPI DSI test for the 4-inch ST7703 panel. It powers the DSI PHY through on-chip LDO channel 3, resets the panel on GPIO 27, controls the backlight on GPIO 26, and displays the controller's vertical color-bar pattern.

The project uses `waveshare/esp_lcd_st7703` 2.0.0 directly instead of LVGL or the board BSP. It is useful when isolating a panel, timing, reset, or backlight problem.

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

The repository CI targets ESP-IDF v5.5.5 and v6.0.2. A successful compile cannot confirm DSI signal integrity, panel revision, color order, or backlight polarity.
