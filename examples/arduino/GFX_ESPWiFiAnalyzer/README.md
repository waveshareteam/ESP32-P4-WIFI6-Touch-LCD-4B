# GFX_ESPWiFiAnalyzer

[中文](README_ZH.md)

Scans nearby 2.4 GHz Wi-Fi networks and plots channel occupancy and signal
strength on the 720 x 720 display.

## Dependencies

- Arduino-ESP32 `3.3.11`, including its `WiFi` and `Network` libraries
- GFX Library for Arduino `1.6.6`
- The repository-local `Waveshare_ESP32_P4_4B_Display` helper

## Board settings

Enable PSRAM, select 32 MB flash with the 13 MB application partition, and use
`ChipVariant=postv3`. Keep the USB mode, CDC-on-boot, MSC, DFU, and upload-mode
selections at their defaults.

## Wireless boundary

ESP32-P4 has no integrated Wi-Fi radio. Runtime Wi-Fi depends on the onboard
ESP32-C6 running firmware compatible with the Arduino-ESP32 host stack. A
successful host build does not prove that the C6 protocol and firmware match.
This sketch has not been validated on a physical board in this repository.
