# Drawing_board

[中文](README_ZH.md)

Polls the GT911 touch controller and draws blue points on the 720 x 720 MIPI
DSI panel, providing a minimal display-and-touch interaction check.

## Dependencies

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- The repository-local `Waveshare_ESP32_P4_4B_Display` helper

## Board settings

Enable PSRAM, select 32 MB flash with the 13 MB application partition, and use
`ChipVariant=postv3`. Keep the USB mode, CDC-on-boot, MSC, DFU, and upload-mode
selections at their defaults.

## Hardware boundary

Touch uses SDA GPIO7 and SCL GPIO8. The helper does not drive GT911 INT or RST:
it probes `0x5D` first, then `0x14`, and initializes the address that responds.
The interrupt is not connected to an ESP32-P4 GPIO in the referenced schematic,
so this sketch polls the controller. It has not been validated on a physical
board in this repository.
