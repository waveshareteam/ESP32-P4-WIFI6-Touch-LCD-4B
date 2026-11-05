# LVGL v9 Arduino

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Runs LVGL v9 widgets with GT911 touch input.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-4B development board

## How to Run

1. Install Arduino-ESP32 3.3.11 and select ESP32P4 Dev Module.
2. Enable PSRAM, select 32 MB flash, and select Chip Variant v3.00 or newer.
3. Upload `04_LVGLV9_Arduino.ino`.

## Expected Result

The LVGL interface responds to touch.

## Troubleshooting

The driver probes GT911 I2C addresses `0x5D` and `0x14`.
