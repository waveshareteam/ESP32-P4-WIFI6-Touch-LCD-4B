# SD Card

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Initializes the on-board MicroSD slot and performs file operations.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-4B development board
- MicroSD card

## How to Run

1. Install Arduino-ESP32 3.3.11 and select ESP32P4 Dev Module.
2. Enable PSRAM, select 32 MB flash, and select Chip Variant v3.00 or newer.
3. Insert a formatted card and upload `08_SD_Card.ino`.

## Expected Result

The serial monitor reports card and file operation results.

## Troubleshooting

Use a correctly formatted MicroSD card.
