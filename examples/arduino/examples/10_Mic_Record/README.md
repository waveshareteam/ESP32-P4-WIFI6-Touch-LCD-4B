# Microphone Record

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Captures audio from the ES7210 microphones and reports sample data over serial.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-4B development board

## How to Run

1. Install Arduino-ESP32 3.3.11 and select ESP32P4 Dev Module.
2. Enable PSRAM, select 32 MB flash, and select Chip Variant v3.00 or newer.
3. Upload `10_Mic_Record.ino` and open the serial monitor.

## Expected Result

The serial monitor reports captured audio values.

## Troubleshooting

Use the configured serial baud rate from the sketch.
