# Drawing Board

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Uses the GT911 touch controller to draw touch strokes on the display.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-4B development board

## How to Run

1. Install Arduino-ESP32 3.3.11 and select ESP32P4 Dev Module.
2. Enable PSRAM, select 32 MB flash, and select Chip Variant v3.00 or newer.
3. Upload `03_Drawing_board.ino`.

## Expected Result

Touch input draws lines on the screen.

## Troubleshooting

The driver probes GT911 I2C addresses `0x5D` and `0x14`.
