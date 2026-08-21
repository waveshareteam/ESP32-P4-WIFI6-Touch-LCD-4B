# Display and touch panel

[中文](README_ZH.md)

Starts the Waveshare BSP 3.0.1 display stack with LVGL 9.3.0 and `espressif/esp_lvgl_adapter` 0.6.3, shows red/green/blue/white screens, then creates a full-screen LVGL touch area that draws black squares at GT911 touch positions.

The project is fixed to the 4-inch 720 x 720 panel selection. All LVGL object changes are protected by the BSP display lock; the BSP owns the LVGL timer task.

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

Use this as the combined LCD/touch smoke test. Panel output, coordinate orientation, multi-touch behavior, and drawing latency require testing on the board and are not established by CI compilation.
