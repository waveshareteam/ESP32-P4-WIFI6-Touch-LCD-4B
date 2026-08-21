[简体中文](README_ZH.md) · [Repository home](../../README.md)

# Arduino Examples

Arduino sketches and bundled libraries for the Waveshare ESP32-P4-WIFI6-Touch-LCD-4B
(720 × 720 ST7703 MIPI-DSI display, GT911 capacitive touch, OV5647 MIPI-CSI camera).

## Board settings (Arduino IDE)

- Arduino-ESP32 3.3.11 (or newer 3.x).
- Board: `ESP32P4 Dev Module` (`esp32:esp32:esp32p4`).
- Menu options:
  - `Chip Variant`: `v3.00 or newer` (the Arduino menu label for the default
    `postv3` setting; this repository validates rev3.x, v3.00-v3.99)
  - `PSRAM`: `Enabled`
  - `Flash Size`: `32 MB`
  - `Flash Mode`: `QIO`
  - `Flash Frequency`: `80 MHz`
  - `Partition Scheme`: `13M APP / 7M data (32 MB)`
- `Upload Mode`: `Default (USB-UART bridge)`
- The default CI FQBN keeps `USBMode=default` and `CDCOnBoot=default`; use
  `UART0` through the board's `CH343P` USB-UART bridge for serial output.
- Enable PSRAM in the board settings; the display sketches require it.
- For confirmed rev1.x ESP32-P4 silicon (including rev1.3), select
  `Chip Variant: Before v3.00`; this repository validates that setting only for
  rev1.x silicon (v1.00-v1.99), and it uses 200 MHz PSRAM. This is
  a silicon setting, not a PCB revision. Do not mix the two profiles.

## Examples

| Sketch | Description |
| --- | --- |
| `01_HelloWorld` | Minimal Arduino_GFX DSI display bring-up |
| `02_AsciiTable` | Arduino_GFX capability/benchmark table |
| `03_Drawing_board` | GT911 five-point capacitive touch drawing |
| `04_LVGLV9_Arduino` | LVGL 9 widgets UI with touch input |
| `05_GFX_ESPWiFiAnalyzer` | Graphical Wi-Fi scan (on-board ESP32-C6 coprocessor) |
| `06_Camera_Preview` | OV5647 MIPI-CSI camera preview on the display |
| `07_Camera_ISP_Tuning` | Live preview with interactive ISP/3A tuning over serial |
| `08_SD_Card` | microSD read/write over the SDIO 3.0 slot |
| `09_Audio_Playback` | ES8311 codec plays the opening of "Für Elise" as different-frequency tones |
| `10_Mic_Record` | ES7210 quad-microphone capture; prints peak/RMS/decimated samples over serial |

## Audio notes

`09_Audio_Playback` drives the ES8311 codec (I2C 0x18) over TX-only I2S
(MCLK 13, BCLK 12, LRCK 10, DOUT 9) and enables the 2 W speaker via GPIO 53.
`10_Mic_Record` captures the on-board ES7210 microphones (I2C 0x40) over RX-only
I2S (DIN 11) at 16 kHz/16-bit and prints the recorded frames to the serial monitor.

## Camera notes

The `06_Camera_Preview` and `07_Camera_ISP_Tuning` sketches use the `ESP_Video` library
bundled with the Arduino-ESP32 core (MIPI-CSI device). An OV5647 module must be
connected to the board MIPI-CSI connector. The default sensor mode streams RAW8
frames which the ISP pipeline converts to RGB565 for the display. `07_Camera_ISP_Tuning`
adds interactive controls over the serial monitor: `g` gain, `e` exposure (µs),
`a` AE target level, `v/h` flip, `t` test pattern, `s` status.

## Touch notes

The bundled GT911 driver probes both legal I2C addresses, `0x5D` and `0x14`,
because the current Arduino configuration does not actively drive touch RST/INT.
If neither address responds, touch examples report the condition and continue
without touch instead of aborting. Coordinate behavior still requires board testing.

## Bundled libraries

- `displays/` — board display/touch/I2C configuration and drivers (ST7703 DSI init,
  GT911 touch, non-blocking serial log)
- `GFX_Library_for_Arduino` — Arduino_GFX with ESP32-P4 MIPI-DSI panel support
- `lvgl` + `lv_conf.h` — LVGL 9 for the `04_LVGLV9_Arduino` sketch

See the [main README](../../README.md) and the [official product documentation]
(https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B) for hardware details.
