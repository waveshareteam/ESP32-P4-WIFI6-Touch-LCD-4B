# LVGL 8 widgets

[中文](README_ZH.md)

Runs the LVGL 8.4.0 widget demo through `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.1 and `espressif/esp_lvgl_adapter` 0.6.3 on the 4-inch 720 x 720 panel.

The BSP starts the display, touch input, LVGL adapter, and LVGL task. Application UI creation is performed while holding the BSP display lock.

BSP 3.0.1 exposes the LVGL 9 rotation type spelling in its display API. This project applies a compile-only alias to LVGL 8's equivalent type; it does not fork or modify the managed BSP.

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

The repository CI targets ESP-IDF v5.5.5 and v6.0.2. Visual output, touch orientation, tearing, frame rate, and memory headroom remain hardware-validation items.
