# LCD Color Test

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Runs a 720 x 720 MIPI-DSI color-bar test using Board Manager's automatic JD9365DA/ST7703 panel selection.

## Hardware Required

An ESP32-P4-WIFI6-Touch-LCD-4B development board is required.

## How to Run

Use a supported ESP-IDF version from the [product configuration](../../../.github/product.json). Board Manager uses the shared board definition.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## Expected Result

The panel displays the MIPI-DSI host's vertical color-bar pattern.

## Troubleshooting

Check the selected panel in the boot log and the MIPI-DSI cable connection.
