# LVGL 9 benchmark

[中文](README_ZH.md)

Runs the LVGL 9.3.0 benchmark through `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.0 and `espressif/esp_lvgl_adapter` 0.6.3 on the 4-inch 720 x 720 panel. Backlight brightness is set to 50 percent after the BSP display stack starts.

The BSP owns display flushing, touch registration, and the LVGL task. Application UI creation is performed while holding the BSP display lock.

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

The repository CI targets ESP-IDF v5.5.5 and v6.0.2. Benchmark scores, tearing, touch behavior, thermals, and memory headroom must be measured on hardware.
