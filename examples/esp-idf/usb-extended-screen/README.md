# USB Extended Screen

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Turns the board into a 720 x 720 USB auxiliary display using Espressif's vendor USB display protocol. The default composite device also provides touch and USB audio interfaces.

## Hardware Required

An ESP32-P4-WIFI6-Touch-LCD-4B development board, a data-capable USB cable, and a Windows host with a compatible indirect display driver are required.

## How to Run

Use ESP-IDF v5.5.6 or v6.1; Board Manager resolves `waveshare/waveshare_boards` >=0.3.1 and `espressif/esp_board_manager` >=0.7.5 as dependencies.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## Expected Result

With the compatible host driver installed, the host presents the board as a 720 x 720 auxiliary display.

## Troubleshooting

Install and configure the compatible driver according to the [Espressif USB extended-screen documentation](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_extend_screen).
