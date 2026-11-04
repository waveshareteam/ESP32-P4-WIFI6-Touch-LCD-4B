# Wi-Fi Analyzer

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Scans Wi-Fi networks through the ESP32-C6 coprocessor and displays the results.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-4B development board

## How to Run

1. Install Arduino-ESP32 3.3.11 and select ESP32P4 Dev Module.
2. Enable PSRAM, select 32 MB flash, and select Chip Variant v3.00 or newer.
3. Upload `05_GFX_ESPWiFiAnalyzer.ino`.

## Expected Result

The display lists detected Wi-Fi networks.

## Troubleshooting

The ESP32-C6 requires compatible hosted firmware.
