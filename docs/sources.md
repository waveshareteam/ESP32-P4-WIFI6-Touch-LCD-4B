# Official Sources and Import Record

[English](sources.md) · [简体中文](sources_ZH.md)

This repository normalizes material from the official Waveshare resources for
the ESP32-P4-WIFI6-Touch-LCD-4B. The source archive is intentionally not
committed; the table below records the exact files used for this import.

Retrieval date: **2026-07-30**

## Product documentation

- [Product documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B)
- [Resources and documents](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B/Resources-And-Documents)
- [ESP-IDF development guide](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B/Development-Environment-Setup-IDF)
- [FAQ](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B/FAQ)

## Downloaded resources

| Resource | Official URL | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Product examples ZIP | [ESP32-P4-WIFI6-Touch-LCD-4B.zip](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/ESP32-P4-WIFI6-Touch-LCD-4B.zip) | 117633366 | `f9b7ec05e1e7d8f955e8af7104cbb8b86f0b2a46c594296449693dff05d6debe` |
| 2D drawing ZIP | [ESP32-P4-WIFI6-Touch-LCD-4B-2D.zip](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/ESP32-P4-WIFI6-Touch-LCD-4B-2D.zip) | 132555 | `a7efcb63c2f4ea5fa74144b73a459368b1f3c56b72504a60b7b591650d4282f2` |
| Main-board schematic | [ESP32-P4-WIFI6-Touch-LCD-4B.pdf](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/ESP32-P4-WIFI6-Touch-LCD-4B.pdf) | 3166842 | `24b9b96d06256dfc613d9a555ae34cb4290b119a3830ac40a07846f81b1e70a1` |
| Current official 86-panel bottom-board schematic | [86_Panel_Bottom_Board.pdf](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/86_Panel_Bottom_Board.pdf) | 476873 | `167957dbba2b13f782cbf8319dc491a0228d0f509d35a6a28152035afb08a44f` |

The 2D archive was extracted to `hardware/dimensions/`:

| Repository file | Bytes | SHA-256 |
| --- | ---: | --- |
| `hardware/dimensions/ESP32-P4-WIFI6-Touch-LCD-4B.dxf` | 997746 | `59c026d9a5ed55ee2466288849a4e898ab87f28d39555470e724d69639adad51` |
| `hardware/dimensions/ESP32-P4-WIFI6-Touch-LCD-4B.pdf` | 26445 | `e690fa9c3c48ec62b1c8167a1d9bd8e9bf44d7b6af4b20f109eb97826c4d1a25` |

`schematic/86 Panel Bottom Board V1.3(1).pdf` was already present before the
official-resource import. Its SHA-256 is
`23b3dad5b025ac0310a8d17e6716308e71fb2c17d69e082f51df06f23651b86a`
and its size is 183700 bytes. No exact official download URL was verified for
that copy during this import, so it is retained as a historical V1.3
reference, not presented as the current download.

## Example normalization

| Official archive path | Repository path | Import treatment |
| --- | --- | --- |
| `ESP-IDF/01_HowToCreateProject` | `examples/esp-idf/project-template` | Normalized name |
| `ESP-IDF/02_HelloWorld` | `examples/esp-idf/hello_world` | Repository-maintained equivalent |
| `ESP-IDF/03_i2c_tools` | `examples/esp-idf/i2c-tools` | Normalized name |
| `ESP-IDF/04_sdmmc` | `examples/esp-idf/sd-card` | Normalized name |
| `ESP-IDF/05_wifistation` | `examples/esp-idf/wifi-station` | Normalized name |
| `ESP-IDF/06_I2SCodec` | `examples/esp-idf/audio-codec` | Normalized name |
| `ESP-IDF/07_color_panel` | `examples/esp-idf/lcd-color-test` | Normalized name |
| `ESP-IDF/08_lvgl_display_panel` | `examples/esp-idf/display-panel` | Normalized name |
| `ESP-IDF/09_lvgl_demo_v8` | `examples/esp-idf/lvgl-v8` | Normalized name |
| `ESP-IDF/10_lvgl_demo_v9` | `examples/esp-idf/lvgl-v9` | Normalized name |
| `ESP-IDF/11_esp_brookesia_phone` | `firmware/brookesia` | Existing maintained firmware retained instead of duplicating the archive project |
| `ESP-IDF/12_usb_extend_screen` | `examples/esp-idf/usb-extended-screen` | Normalized name |
| `ESP-IDF/13_ethernetbasic` | `examples/esp-idf/ethernet` | Normalized name |
| `ESP-IDF/14_RS485_Test` | `examples/esp-idf/rs485` | Normalized name |
| `Arduino/AsciiTable` | `examples/arduino/AsciiTable` | Imported first-party sketch |
| `Arduino/Drawing_board` | `examples/arduino/Drawing_board` | Imported first-party sketch |
| `Arduino/GFX_ESPWiFiAnalyzer` | `examples/arduino/GFX_ESPWiFiAnalyzer` | Imported first-party sketch |
| `Arduino/HelloWorld` | `examples/arduino/HelloWorld` | Imported first-party sketch |
| `Arduino/LVGLV9_Arduino` | `examples/arduino/LVGLV9_Arduino` | Imported first-party sketch |

The official Arduino archive bundles GFX Library for Arduino 1.6.0 and LVGL
9.3.0. The repository pins GFX Library for Arduino 1.6.6 to follow the current
upstream ESP32-P4 clock-divider API while retaining LVGL 9.3.0. This repository
keeps the product-specific display/touch helper under
`examples/arduino/libraries/`. Whether those large generic libraries are
vendored or installed by the build environment does not change the versions
selected by the repository matrix.

## Provenance versus validation

Hashes verify which source files were retrieved; they do not establish
copyright permission, runtime compatibility, or hardware correctness. Imported
code is being normalized and may differ from the ZIP after repository
maintenance. See `docs/licensing.md` for redistribution boundaries and
`docs/ci.md` for compile-validation status.
