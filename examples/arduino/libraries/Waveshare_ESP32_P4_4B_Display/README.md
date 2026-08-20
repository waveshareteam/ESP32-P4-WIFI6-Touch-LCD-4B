# Waveshare ESP32-P4 WIFI6 Touch LCD 4B Display Support

[简体中文](README_ZH.md)

This is the board-specific Arduino helper imported from the official
ESP32-P4-WIFI6-Touch-LCD-4B example archive. It contains the 720 x 720
ST7703 panel initialization and timing data, plus the shared I2C and GT911
touch implementation used by the first-party sketches in this repository.

## Hardware mapping

- MIPI DSI panel: 720 x 720, 2 lanes, 480 Mbps per lane
- LCD reset: GPIO27
- Touch controller: GT911; the helper probes I2C address `0x5D` first, then
  `0x14`, and initializes the responding address
- Touch I2C: SDA GPIO7, SCL GPIO8
- Touch reset and interrupt: not driven by the helper; the interrupt is not
  connected to an ESP32-P4 GPIO in the referenced board schematic

The timing and initialization sequence in `displays_config.h` are kept from
the official product package. The display name was normalized to the product
name when the helper was imported.

GT911 input is read by polling because no interrupt GPIO is configured. The
address probe and polling path are software behavior only; physical address
selection timing and touch operation still require hardware validation.

## Arduino dependencies

Repository CI pins these versions:

- Arduino-ESP32 `3.3.11`
- GFX Library for Arduino `1.6.6`
- LVGL `9.3.0` for the LVGL sketch only

The official product archive bundled GFX Library for Arduino `1.6.0` and LVGL
`9.3.0`. GFX `1.6.6` is used here for compatibility with the pinned current
Arduino-ESP32 core. The board helper itself does not contain LVGL code.

## Source and licensing boundary

- Product documentation: https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B
- Official example archive: https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/ESP32-P4-WIFI6-Touch-LCD-4B.zip

The imported `gt911.h` and `gt911.cpp` files carry their own Apache-2.0 SPDX
notices. The other files in this helper do not declare a license in the
official archive. This repository does not infer or grant a license for those
files; preserve all file-level notices and consult Waveshare for terms.
