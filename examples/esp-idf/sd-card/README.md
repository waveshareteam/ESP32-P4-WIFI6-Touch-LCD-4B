# MicroSD Card

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Mounts a FAT filesystem on a microSD card, writes and reads a test file, then unmounts the card.

## Hardware Required

An ESP32-P4-WIFI6-Touch-LCD-4B development board and a microSD card are required.

## How to Run

Use ESP-IDF v5.5.6 or v6.1; Board Manager resolves `waveshare/waveshare_boards` >=0.3.1 and `espressif/esp_board_manager` >=0.7.5 as dependencies.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## Expected Result

The monitor reports that the card was mounted and that the test file was written and read.

## Troubleshooting

Back up the card before enabling any formatting option in `idf.py menuconfig`.
