# LVGLV9_Arduino

[English](README.md) · [简体中文](README_ZH.md)

Runs a self-contained LVGL 9 interface on the 720 × 720 MIPI DSI display with
GT911 pointer input and two partial DMA draw buffers. The interface uses only
public LVGL core widgets so the sketch works with the Arduino Library Manager
package; it does not require the upstream source repository's separate
`demos/` directory.

## Dependencies

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- LVGL `9.3.0`
- The repository-local `Waveshare_ESP32_P4_4B_Display` helper

The adjacent `lv_conf.h` is the product package configuration for LVGL 9.3.0.
Review and migrate that configuration before changing the LVGL version.

## Board settings

Enable PSRAM, select 32 MB flash with the 13 MB application partition, and use
`ChipVariant=prev3`. Keep the USB mode, CDC-on-boot, MSC, DFU, and upload-mode
selections at their defaults.

## Runtime safeguards

Display, touch, LVGL object, DMA-buffer, and tick-timer initialization failures
stop setup and report the first error on the serial console. The draw-buffer
size passed to LVGL is measured in bytes, matching the LVGL 9 API contract.

## Hardware boundary

Touch uses SDA GPIO7, SCL GPIO8, and reset GPIO23 and is polled because the
touch interrupt is not connected to an ESP32-P4 GPIO in the referenced
schematic. Compilation does not validate display timing, touch behavior, or
memory stability on a physical board.
