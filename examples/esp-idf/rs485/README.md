# RS485 Echo

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Receives data through the RS485 transceiver on the compatible bottom board and echoes it back to the bus.

## Hardware Required

Use an ESP32-P4-WIFI6-Touch-LCD-4B board with the compatible ESP32-P4-86-Panel-ETH-2RO bottom board and an RS485 peer.

## How to Run

Use ESP-IDF v5.5.3, v5.5.6, or v6.1; Board Manager resolves `waveshare/waveshare_boards` >=0.3 and `espressif/esp_board_manager` >=0.7.5 as dependencies.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## Expected Result

Data received from the RS485 peer is echoed and reported in the monitor.

## Troubleshooting

Connect A to A and B to B, and use an appropriate common reference and termination for the RS485 network.
