# Wi-Fi Station Through ESP32-C6

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Runs the ESP-IDF station example on ESP32-P4 through the board's ESP32-C6 coprocessor. Set the network credentials in `idf.py menuconfig` before building.

## Hardware Required

An ESP32-P4-WIFI6-Touch-LCD-4B development board and access to a Wi-Fi network are required.

## How to Run

Use ESP-IDF v5.5.6 or v6.1; Board Manager resolves `waveshare/waveshare_boards` >=0.3.1 and `espressif/esp_board_manager` >=0.7.5 as dependencies.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## Expected Result

After connecting, the monitor reports the station's network events and assigned address.

## Troubleshooting

Confirm the network credentials and matching ESP-Hosted firmware on the ESP32-C6 before retrying the connection.
