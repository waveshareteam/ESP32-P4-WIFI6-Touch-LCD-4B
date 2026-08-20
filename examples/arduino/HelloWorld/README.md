# HelloWorld

[中文](README_ZH.md)

Initializes the 720 x 720 MIPI DSI panel, prints a greeting, and then draws the
same text at randomized positions, colors, and sizes.

## Dependencies

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- The repository-local `Waveshare_ESP32_P4_4B_Display` helper

## Board settings

Enable PSRAM, select 32 MB flash with the 13 MB application partition, and use
`ChipVariant=postv3`. Keep the USB mode, CDC-on-boot, MSC, DFU, and upload-mode
selections at their defaults.

## Hardware boundary

This display-only sketch does not use touch or the ESP32-C6 wireless
coprocessor. It has not been validated on a physical board in this repository.
