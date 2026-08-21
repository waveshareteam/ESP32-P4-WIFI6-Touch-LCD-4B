# Board Information

[English](board.md) · [简体中文](board_ZH.md)

This document separates the standalone ESP32-P4-WIFI6-Touch-LCD-4B from the
related ESP32-P4-86-Panel-ETH-2RO assembly and its bottom board. The official
product page identifies different main-PCB and enclosure arrangements; the
bottom board is not documented as a drop-in option for the standalone 4B.
Pin facts come from the official product documentation, the schematics listed
in `schematic/README.md`, and the current board-support configuration. A
documented pin is not, by itself, evidence that a peripheral has been exercised
on physical hardware.

## Main display board

| Item | Value |
| --- | --- |
| Product | ESP32-P4-WIFI6-Touch-LCD-4B |
| ESP-IDF target | `esp32p4` |
| Default example baseline | ESP32-P4 rev3.x / post-v3 (`SELECTS_REV_LESS_V3=n`, 3.0 minimum, 250 MHz PSRAM) |
| Maintained firmware profiles | Separate `rev1_3` and `rev3_x` Brookesia silicon/configuration profiles |
| Flash | 32 MB |
| PSRAM | 32 MB, Hex mode in the maintained Brookesia defaults |
| Display | 4-inch, 720 x 720, two-lane MIPI DSI, ST7703 |
| Touch | GT911 capacitive touch |
| Playback codec | ES8311 |
| Recording codec | ES7210 |
| Wireless coprocessor | ESP32-C6-MINI-1U-H8 over SDIO |
| Camera | MIPI CSI connector; current firmware defaults to an OV5647 mode |
| Storage | Four-bit SDMMC MicroSD slot |

### Display and touch

| Function | Signal | GPIO / setting |
| --- | --- | --- |
| LCD | Resolution | 720 x 720 |
| LCD | Interface | Two-lane MIPI DSI |
| LCD | DSI lane rate in Registry BSP 3.0.1 | 480 Mbit/s per lane |
| LCD | Reset | GPIO27 |
| LCD backlight | PWM | GPIO26 |
| LCD backlight | Enable | GPIO33 |
| Touch / shared control bus | SDA | GPIO7 |
| Touch / shared control bus | SCL | GPIO8 |
| Touch | Reset | GPIO23 is present in the schematic; neither the Arduino helper nor the published BSP 3.0.1 drives it |
| Touch | Interrupt | Test point only; not connected to an MCU GPIO and not used by the Arduino helper or published BSP 3.0.1 |

The LCD reset and touch reset are separate signals on this product. The Arduino
helper deliberately does not drive either GT911 INT or RST: it probes I2C
address `0x5D` first, then `0x14`, initializes the responding address, and
polls touch state. The published BSP 3.0.1 uses the same behavior. Do not copy a
shared-reset assumption from another Waveshare board. Compilation does not
validate address-selection timing or touch input on physical hardware.

### Audio

| Signal | GPIO |
| --- | ---: |
| Shared codec I2C SDA / SCL | 7 / 8 |
| I2S playback data out | 9 |
| I2S LRCK / word select | 10 |
| I2S record data in | 11 |
| I2S bit clock | 12 |
| I2S master clock | 13 |
| Power-amplifier enable | 53 |

The maintained firmware uses ES8311 for playback and ES7210 for recording.
Its board-observed ES7210 serialized TDM order is MIC1, MIC3 (echo reference),
MIC2, MIC4. Physical microphone-selection masks and serialized TDM slot masks
are different namespaces and must not be interchanged.

### MicroSD

| Signal | GPIO |
| --- | ---: |
| D0 | 39 |
| D1 | 40 |
| D2 | 41 |
| D3 | 42 |
| CLK | 43 |
| CMD | 44 |
| Power control | 45 |

### ESP32-C6 wireless coprocessor

The P4 host uses GPIO6, GPIO14, GPIO16, GPIO18, GPIO19, GPIO20, GPIO22, and
GPIO54 for the C6 SDIO/control connection. The signal-level mapping and timing
are owned by the Hosted configuration and matching C6 firmware; do not reuse
those pins as general-purpose GPIO while wireless support is enabled.

The official example archive does not contain the C6 slave source, binary, or
build instructions. See `docs/p4-c6-hosted-wifi.md`.

### Camera and shared buses

The camera uses the dedicated MIPI CSI interface and shares the GPIO7/GPIO8
I2C control bus. Verify sensor voltage, ribbon orientation, and the selected
sensor mode before enabling a camera not supplied for this board.

## 86-panel bottom board

Ethernet, relays, and RS485 are on the bottom board of the related 86-panel
product assembly. They are not features of the standalone 4B board, and this
repository does not claim that the two main-PCB or enclosure arrangements are
interchangeable.

| Function | Signal | GPIO / setting |
| --- | --- | --- |
| Relay 1 | Active-high output | GPIO32 |
| Relay 2 | Active-high output | GPIO46 |
| RS485 | UART1 TX | GPIO47 |
| RS485 | UART1 RX | GPIO48 |
| RS485 | Example serial format | 115200, 8 data bits, no parity, 1 stop bit |
| IP101 | PHY address | 1 |
| IP101 | Reset | GPIO51 |
| IP101 | MDC / MDIO | GPIO31 / GPIO52 |
| IP101 | RMII clock input | GPIO50 |
| IP101 | TX_EN / TXD0 / TXD1 | GPIO49 / GPIO34 / GPIO35 |
| IP101 | CRS_DV / RXD0 / RXD1 | GPIO28 / GPIO29 / GPIO30 |

The current official product documentation describes the RS485 interface as
using automatic direction control; the application therefore uses UART TX/RX
without a separate direction GPIO. Confirm the actual bottom-board revision
before relying on receive operation.

Project-specific relay, Ethernet, and RS485 composition belongs in
`bsp_extra`; it should not be moved into a reusable display/touch BSP without a
matching shared-board design.

### Historical V1.3 RS485 difference

In the reviewed historical file
`schematic/86 Panel Bottom Board V1.3(1).pdf`, the RS485 transceiver `/RE` and
`DE` signals are tied together and the net is pulled high through 4.7 kOhm.
That state selects transmit mode and disables the receiver. Receive operation
may therefore be unavailable on that V1.3 board.

This is a revision-specific schematic conclusion. Confirm the actual bottom
board and the current official schematic before applying it to another
revision.

Do not short RS485 A and B together for loopback testing. That collapses the
differential bus. Do not connect another actively driving RS485 adapter until
direction control and bus ownership are understood.

## BSP baseline

The repository migration baseline is the published
`waveshare/esp32_p4_wifi6_touch_lcd_4b` version 3.0.1:

- ESP-IDF `>=5.5`.
- `espressif/esp_lvgl_adapter ~0.6`.
- `waveshare/esp_lcd_st7703 ^2.0.0`.
- `espressif/esp_lcd_touch_gt911 ^1`.
- `espressif/esp_codec_dev ~1.5`.
- `espressif/usb ^1.0.0` when building with ESP-IDF 6 or newer.

First-party ESP-IDF examples that need board or display support, together with
the maintained Brookesia firmware, resolve the reusable BSP and ST7703 driver
from the ESP Component Registry. No local copy of either component is kept to
shadow that resolution; only product-specific `bsp_extra` code remains local.

Product manifests resolve the published BSP 3.0.1 through the Component Registry.
Do not use a Git URL as a substitute for a registry release.

## Validation status

`rev1_3` and `rev3_x` name silicon/configuration profiles, not verified PCB
electrical revisions. `rev1_3` is pre-v3, minimum silicon revision 1.00, with
a 200 MHz PSRAM baseline; `rev3_x` is post-v3, minimum silicon revision 3.00,
with a 250 MHz PSRAM baseline. Do not cross-flash their binaries. The default
ESP-IDF examples and Arduino sketches use `rev3_x`/post-v3 and are not doubled
into a second matrix. Brookesia alone continues to build both profiles. Its
v3.x profile requires ESP-IDF 5.5.3 or newer (or 6.0 or newer), while this
repository's maintained firmware workflow currently pins v5.5.5. The available
main-board schematics do not establish a PCB/electrical difference between
these two profile names; a chip probe or compile likewise does not prove
hardware compatibility.

### Supported by source material

- Main-board and bottom-board pin tables above.
- Display controller, resolution, touch controller, codecs, C6 module, flash,
  and PSRAM capacities.
- Historical V1.3 RS485 direction-net topology.

### Compile validation

A successful ESP-IDF or Arduino build checks source/API compatibility for the
selected framework version. The workflow result and its exact dependency
resolution should be recorded separately; this document does not claim that a
build has passed merely because a matrix entry exists.

### Hardware validation still required

- Confirm main-board, bottom-board, and ESP32-P4 silicon revisions.
- Display, touch, backlight, speaker, all connected microphones, and audio
  clocking.
- Camera sensor and supported modes.
- MicroSD, USB, C6 Wi-Fi, IP101 Ethernet, relays, and RS485 behavior.
- The exact C6 slave firmware and compatible P4 host stack.

When recording results, include the hardware revision, framework version,
example path, dependency versions, and whether the test was compile-only or
performed on a physical board.
