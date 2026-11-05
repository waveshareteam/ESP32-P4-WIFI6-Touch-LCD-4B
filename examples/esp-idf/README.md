# ESP-IDF Examples

[中文](README_ZH.md)

This directory contains the normalized first-party ESP-IDF projects from the
official ESP32-P4-WIFI6-Touch-LCD-4B product example archive. Projects are
immediate children of this directory so build discovery does not accidentally
include tests or examples nested inside components.

| Project | Official source | Purpose | Hardware or runtime requirement |
| --- | --- | --- | --- |
| [project-template](project-template/) | `01_HowToCreateProject` | Minimal project structure | None beyond the ESP32-P4 |
| [hello_world](hello_world/) | `02_HelloWorld` equivalent | Minimal application and CI smoke test | None beyond the ESP32-P4 |
| [i2c-tools](i2c-tools/) | `03_i2c_tools` | Interactive I2C scanning and register access | Use documented shared I2C pins carefully |
| [sd-card](sd-card/) | `04_sdmmc` | MicroSD card access | Main-board MicroSD slot |
| [wifi-station](wifi-station/) | `05_wifistation` | Station-mode Wi-Fi through ESP32-C6 | SSID/password and compatible C6 firmware |
| [audio-codec](audio-codec/) | `06_I2SCodec` | ES8311 playback/audio path | Main-board audio hardware |
| [lcd-color-test](lcd-color-test/) | `07_color_panel` | ST7703 panel color test | Main-board display |
| [display-panel](display-panel/) | `08_lvgl_display_panel` | BSP-backed display/touch UI | Main-board display and touch |
| [lvgl-v9](lvgl-v9/) | `10_lvgl_demo_v9` | LVGL 9 demonstration | Main-board display and touch |
| [usb-extended-screen](usb-extended-screen/) | `12_usb_extend_screen` | USB display/audio/touch composition | USB host and device configuration |
| [ethernet](ethernet/) | `13_ethernetbasic` | IP101 Ethernet | Matching ESP32-P4-86-Panel-ETH-2RO assembly |
| [rs485](rs485/) | `14_RS485_Test` | UART/RS485 transmit and receive | Matching ESP32-P4-86-Panel-ETH-2RO assembly; revision caveat below |

The official `11_esp_brookesia_phone` project is represented by the maintained
`examples/factory/` source tree rather than duplicated here.

## Build matrix

Supported framework versions are listed in the [product configuration](../../.github/product.json).
The open factory source is maintained separately in [examples/factory](../factory/README.md).

Examples default to the post-v3 silicon profile, with memory clocks supplied by the board definition. The factory defaults to rev1.x silicon (1.00–1.99, 200 MHz PSRAM). Silicon profiles and PCB electrical revisions are separate.

Activate the intended IDF version explicitly, then build an example:

```sh
python -m pip install --upgrade esp-bmgr-assist
idf.py -C examples/esp-idf/hello_world bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

For a hardware example, replace `hello_world` with the project name. Managed
components are downloaded on the first configure/build unless already cached.
Do not edit generated `managed_components/`, `dependencies.lock`, `sdkconfig`,
or `build/` output.

## Board support

Display, touch and audio use the shared `waveshare/waveshare_boards` definition through
Board Manager or the product BSP compatibility layer. The board package selects
the matching panel initialization and timing.

## Hardware limits

- `wifi-station` requires a C6 slave image compatible with the P4 host stack.
  The source or binary for that slave image is not included in the official
  product example archive.
- `ethernet` and `rs485` use circuitry in the related 86-panel assembly, not
  the standalone 4B board. Do not assume its bottom board is a drop-in option
  for the 4B main PCB.
- The reviewed historical V1.3 bottom-board schematic ties RS485 `/RE` and
  `DE` together and pulls the net high. Receive operation may therefore be
  unavailable on that revision. Do not generalize this limitation to an
  unreviewed hardware revision.

See `docs/board.md` for pins and `docs/p4-c6-hosted-wifi.md` for the wireless
contract. A successful shared Build workflow run does not establish hardware
validation.
