# Camera Preview

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Shows an OV5647 MIPI-CSI camera preview on the display.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-4B development board
- OV5647 MIPI-CSI camera module

## How to Run

1. Install Arduino-ESP32 3.3.11 and select ESP32P4 Dev Module.
2. Enable PSRAM, select 32 MB flash, and select Chip Variant v3.00 or newer.
3. Connect the camera and upload `06_Camera_Preview.ino`.

## Expected Result

The camera image appears on the display.

## Troubleshooting

Check the MIPI-CSI ribbon orientation before powering the board.
