# Factory Firmware

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Builds the ESP32-P4-WIFI6-Touch-LCD-4B factory application and selects the matching ST7703 or JD9365 display profile.

## Hardware Required

An ESP32-P4-WIFI6-Touch-LCD-4B development board is required.

## How to Run

Use the formal factory ESP-IDF v5.5.6 environment; Board Manager resolves `waveshare/waveshare_boards` >=0.3 and `espressif/esp_board_manager` >=0.7.5 as dependencies.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py -B build/rev1_x -DFACTORY_SILICON_PROFILE=rev1_x set-target esp32p4
idf.py -B build/rev1_x -DFACTORY_SILICON_PROFILE=rev1_x build
```

`rev1_x` is the default profile and builds for ESP32-P4 revisions 1.00-1.99.
For revisions 3.00-3.99, use a separate build and SDKCONFIG path:

```bash
idf.py -B build/rev3_x -DFACTORY_SILICON_PROFILE=rev3_x set-target esp32p4
idf.py -B build/rev3_x -DFACTORY_SILICON_PROFILE=rev3_x build
```

Do not switch `FACTORY_SILICON_PROFILE` in an existing build directory. If you
provide `-DSDKCONFIG=...`, give each profile its own SDKCONFIG file.

## Expected Result

The product UI starts with the matching display profile. The application image
version is `factory-rev1x` or `factory-rev3x` for the selected profile.

## Troubleshooting

The repository also builds ESP-IDF examples with v5.5.6 and v6.1; use the [ESP-Hosted tools](https://github.com/espressif/esp-hosted/blob/master/docs/tools.md) for the bundled ESP32-C6 image.
