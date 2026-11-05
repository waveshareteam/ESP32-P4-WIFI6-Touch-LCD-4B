# LCD-4B BSP Compatibility Layer

[English](README.md) | [简体中文](README_ZH.md)

## Purpose

Preserves the factory application's BSP APIs while ESP Board Manager owns the board resources. The hardware definition comes from [waveshare_boards](https://github.com/waveshareteam/waveshare_boards/tree/v0.3.1/boards/esp32_p4_wifi6_touch_lcd_4b).

## Display

The managed definition identifies JD9365DA or ST7703 and selects the matching initialization and DPI timing. GPIO26 controls backlight PWM; GPIO33 remains unused.

## Audio

The compatibility APIs borrow Board Manager's ES8311/ES7210 codec handles. STD output and four-slot TDM input preserve the factory microphone and echo-reference layout.

## Ownership

Returned panel, touch, codec and bus handles are borrowed. Applications release their own state while Board Manager manages the shared hardware lifecycle.

## Build

Follow the board-selection and build commands in the [factory example](../../README.md). Generated Board Manager code is rebuilt locally and is excluded from source control.
