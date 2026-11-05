# I2C Tools

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Provides an interactive I2C console for the board's shared I2C0 bus on GPIO 7 and GPIO 8. It supports bus scanning and register access commands.

## Hardware Required

An ESP32-P4-WIFI6-Touch-LCD-4B development board is required.

## How to Run

Use ESP-IDF v5.5.6 or v6.1; Board Manager resolves `waveshare/waveshare_boards` >=0.3.1 and `espressif/esp_board_manager` >=0.7.5 as dependencies.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## Expected Result

The monitor presents the I2C console prompt and accepts its commands.

## Troubleshooting

Use write commands only when the target device and register are known.
