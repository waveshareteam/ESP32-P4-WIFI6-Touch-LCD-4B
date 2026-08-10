# Arduino Examples

[中文](README_ZH.md)

These five first-party sketches were imported from the official
ESP32-P4-WIFI6-Touch-LCD-4B product example archive.

| Sketch | Purpose | Additional requirements |
| --- | --- | --- |
| `AsciiTable` | Display and text-output demonstration | Display helper, GFX library, PSRAM |
| `Drawing_board` | Touch drawing surface | Display/touch helper, GFX library, PSRAM |
| `GFX_ESPWiFiAnalyzer` | Wi-Fi scan visualization | Display helper, GFX library, compatible ESP32-C6 firmware |
| `HelloWorld` | Basic display bring-up | Display helper, GFX library, PSRAM |
| `LVGLV9_Arduino` | LVGL v9 display and touch demo | Display/touch helper, GFX library, LVGL, local `lv_conf.h`, PSRAM |

Examples shipped inside third-party libraries are not first-party product
sketches and are excluded from product CI.

## Supported toolchain

- Arduino-ESP32 core: `3.3.11`
- FQBN: `esp32:esp32:esp32p4`
- GFX Library for Arduino: `1.6.6`
- LVGL: `9.3.0`
- Local product helper: `libraries/Waveshare_ESP32_P4_4B_Display`

The product helper wraps the ESP-IDF new I2C master driver used by current
Arduino-ESP32 releases and contains the board-specific display/touch setup.

## Board options

The compile baseline is derived from the maintained ESP-IDF board
configuration:

```text
UploadSpeed=921600
FlashFreq=80
FlashMode=qio
FlashSize=32M
PartitionScheme=app13M_data7M_32MB
PSRAM=enabled
ChipVariant=prev3
USBMode=default
CDCOnBoot=default
MSCOnBoot=default
DFUOnBoot=default
UploadMode=default
DebugLevel=none
EraseFlash=none
JTAGAdapter=default
```

The corresponding compile FQBN is:

```text
esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=prev3
```

`ChipVariant=prev3` matches the current repository firmware baseline. Confirm
the actual silicon revision before changing that option for a different
production lot.

## Install dependencies

Install the exact core and library versions when the generic libraries are not
vendored locally:

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GFX Library for Arduino@1.6.6"
arduino-cli lib install "lvgl@9.3.0"
```

## Compile

Pass the repository library directory so the product helper is discoverable:

```sh
arduino-cli compile \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=prev3" \
  --libraries examples/arduino/libraries \
  examples/arduino/HelloWorld
```

Replace `HelloWorld` with any first-party sketch directory. Compilation checks
source compatibility; it does not prove display, touch, Wi-Fi, or other
hardware behavior.

## Wi-Fi boundary

ESP32-P4 has no integrated Wi-Fi. `GFX_ESPWiFiAnalyzer` depends on the
ESP32-C6 coprocessor and its installed slave firmware. The official example
archive does not include that firmware or its build instructions. See
`docs/p4-c6-hosted-wifi.md`.

Source and library provenance is recorded in `docs/sources.md`; licensing
boundaries are recorded in `docs/licensing.md`.
