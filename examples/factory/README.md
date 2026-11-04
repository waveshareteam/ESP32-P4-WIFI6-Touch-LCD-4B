# Factory Firmware

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Builds the factory application for ESP32-P4-WIFI6-Touch-LCD-4B. It detects the
connected MIPI-DSI display controller and initializes the matching ST7703 or
JD9365 panel sequence.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-4B development board

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

Use ESP-IDF v5.5.5. The serial boot log identifies the selected panel.

## Expected Result

The application starts the product UI and uses the matching display profile.

## Troubleshooting

Use the factory firmware's documented ESP-IDF version; other versions are not
part of its build matrix.
