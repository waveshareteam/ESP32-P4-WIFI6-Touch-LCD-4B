[简体中文](README_ZH.md) · [Arduino examples](../../README.md)

# LCD-4B Arduino display helper

This product-owned helper supplies the LCD-4B board layer used by the bundled
Arduino sketches. It configures the 720 x 720 ST7703 two-lane MIPI-DSI panel,
LCD reset on GPIO27, active-low PWM backlight on GPIO26 with enable GPIO33, and
the shared I2C bus on GPIO7/GPIO8. The helper applies Arduino-ESP32 LEDC output
inversion so its brightness polarity matches the published LCD-4B BSP.

The GT911 helper deliberately does not drive the touch INT or RST pins. It
probes I2C address `0x5D` and then `0x14`, uses the responding address, and
polls touch state. A successful compile does not validate touch behavior on
hardware.

The sibling `GFX_Library_for_Arduino` and `lvgl` directories are complete
bundled third-party libraries. They are consumed through the Arduino
`--libraries` option and are not separate product sketches.
