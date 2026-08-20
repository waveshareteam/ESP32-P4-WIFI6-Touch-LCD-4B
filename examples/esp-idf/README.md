# ESP-IDF Examples

[中文](README_ZH.md)

This directory contains the normalized first-party ESP-IDF projects from the
official ESP32-P4-WIFI6-Touch-LCD-4B product example archive. Projects are
immediate children of this directory so build discovery does not accidentally
include tests or examples nested inside components.

| Project | Official source | Purpose | Hardware or runtime requirement |
| --- | --- | --- | --- |
| `project-template` | `01_HowToCreateProject` | Minimal project structure | None beyond the ESP32-P4 |
| `hello_world` | `02_HelloWorld` equivalent | Minimal application and CI smoke test | None beyond the ESP32-P4 |
| `i2c-tools` | `03_i2c_tools` | Interactive I2C scanning and register access | Use documented shared I2C pins carefully |
| `sd-card` | `04_sdmmc` | MicroSD card access | Main-board MicroSD slot |
| `wifi-station` | `05_wifistation` | Station-mode Wi-Fi through ESP32-C6 | SSID/password and compatible C6 firmware |
| `audio-codec` | `06_I2SCodec` | ES8311 playback/audio path | Main-board audio hardware |
| `lcd-color-test` | `07_color_panel` | ST7703 panel color test | Main-board display |
| `display-panel` | `08_lvgl_display_panel` | BSP-backed display/touch UI | Main-board display and touch |
| `lvgl-v8` | `09_lvgl_demo_v8` | LVGL 8 demonstration | Main-board display and touch |
| `lvgl-v9` | `10_lvgl_demo_v9` | LVGL 9 demonstration | Main-board display and touch |
| `usb-extended-screen` | `12_usb_extend_screen` | USB display/audio/touch composition | USB host and device configuration |
| `ethernet` | `13_ethernetbasic` | IP101 Ethernet | Matching ESP32-P4-86-Panel-ETH-2RO assembly |
| `rs485` | `14_RS485_Test` | UART/RS485 transmit and receive | Matching ESP32-P4-86-Panel-ETH-2RO assembly; revision caveat below |

The official `11_esp_brookesia_phone` project is represented by the maintained
`firmware/brookesia/` source tree rather than duplicated here.

## Build matrix

The repository targets ESP-IDF v5.5.5 and v6.0.2 for every first-party example.
Brookesia remains on v5.5.5 until its v6 compatibility is verified.

The default example configuration is the `rev3_x`/post-v3 silicon profile:
minimum ESP32-P4 revision 3.00 and a 250 MHz PSRAM baseline. The retained
`rev1_3` Brookesia profile is an opt-in pre-v3 profile (minimum 1.00, 200 MHz
PSRAM); these are silicon/configuration profiles, not documented PCB electrical
revisions.

Activate the intended IDF version explicitly, then build an example:

```sh
idf.py --version
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

For a hardware example, replace `hello_world` with the project name. Managed
components are downloaded on the first configure/build unless already cached.
Do not edit generated `managed_components/`, `dependencies.lock`, `sdkconfig`,
or `build/` output.

## Board support

Display, touch, and audio examples should use the published
`waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.0 BSP and its managed dependencies.
Project-local code is appropriate only for example composition or
product-specific glue. A BSP 3.0.1 GT911 address-probe update must be published
to the Component Registry before this repository upgrades its manifest; do not
replace the registry dependency with a Git URL.

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

See `docs/board.md` for pins, `docs/p4-c6-hosted-wifi.md` for the wireless
contract, and `docs/ci.md` for the distinction between compile and hardware
validation.
