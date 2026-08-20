# Getting Started

[English](getting-started.md) · [简体中文](getting-started_ZH.md)

## Choose a framework surface

This repository contains:

- ESP-IDF examples under `examples/esp-idf/`.
- Arduino sketches under `examples/arduino/`.
- The larger ESP-Brookesia source firmware under `firmware/brookesia/`.

Start with `hello_world` or Arduino `HelloWorld` before flashing a peripheral
example. Compilation confirms toolchain compatibility; only a physical board
test confirms hardware behavior.

## Hardware prerequisites

- ESP32-P4-WIFI6-Touch-LCD-4B main board.
- A data-capable USB cable.
- A serial port accessible by the host system.
- The complete matching ESP32-P4-86-Panel-ETH-2RO hardware assembly for
  Ethernet, relay, or RS485 examples.
- A compatible ESP32-C6 slave image for Wi-Fi examples.

Confirm the actual board and ESP32-P4 silicon revision before flashing a build
whose board options differ from the repository baseline.

## ESP-IDF prerequisites

The maintained example matrix uses ESP-IDF v5.5.5 and v6.0.2. Brookesia uses
v5.5.5 only. Install and activate one explicit version, then verify it:

```sh
idf.py --version
```

The first configure/build needs access to the ESP Component Registry. Managed
components are generated locally and must not be edited in place.

## Build an ESP-IDF example

```sh
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

Replace `hello_world` with a project listed in
`examples/esp-idf/README.md`. Hardware examples may expose project-specific
settings through `menuconfig`:

```sh
idf.py -C examples/esp-idf/wifi-station menuconfig
```

Do not commit SSIDs, passwords, tokens, certificates, activation identifiers,
or other credentials. Review the first real configure/compiler error rather
than the final cascade when a build fails.

## Flash and monitor an ESP-IDF example

```sh
idf.py -C examples/esp-idf/hello_world -p PORT flash monitor
```

Use the exit sequence printed by ESP-IDF to leave the monitor. A normal flash
uses the project-generated flash arguments; do not guess offsets from another
example.

## Build and flash Brookesia

Use ESP-IDF v5.5.5:

```sh
idf.py --version
idf.py -C firmware/brookesia set-target esp32p4
idf.py -C firmware/brookesia build
idf.py -C firmware/brookesia -p PORT flash monitor
```

The first installation or any partition/model/filesystem change requires the
complete `flash` operation. `app-flash` updates only the application and does
not update the partition table, ESP-SR model image, or SPIFFS storage image.
See `firmware/brookesia/README.md` and `docs/firmware.md`.

## Arduino prerequisites

The maintained Arduino matrix uses:

- Arduino-ESP32 3.3.11.
- FQBN `esp32:esp32:esp32p4`.
- GFX Library for Arduino 1.6.6.
- LVGL 9.3.0 for the LVGL sketch.

Install the exact versions when they are not already available:

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GFX Library for Arduino@1.6.6"
arduino-cli lib install "lvgl@9.3.0"
```

## Compile and upload an Arduino sketch

```sh
arduino-cli compile \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=postv3" \
  --build-path build/arduino/HelloWorld \
  --library examples/arduino/libraries/Waveshare_ESP32_P4_4B_Display \
  examples/arduino/HelloWorld
```

After identifying the correct port:

```sh
arduino-cli upload \
  -p PORT \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=postv3" \
  examples/arduino/HelloWorld
```

For a release candidate, do not publish the generated whole-flash image. Use
the complete privacy-mapped compile and repository packager command in
`examples/arduino/README.md`. It dynamically maps repository, Arduino data/user,
and temporary roots for C, C++, and assembly without changing the FQBN. The
packager reads actual core 3.3.11 `flash_args`, emits a hashed segmented manifest
and `flash.sh`/`flash.cmd`, and never guesses offsets. After segmented flashing,
cold-start once with the CH343P UART0 monitor disconnected, then attach it and
confirm the application does not reset or hang. Compile/package evidence alone
is not this HIL result.

The Wi-Fi analyzer additionally depends on a compatible image already running
on the C6 coprocessor. See `docs/p4-c6-hosted-wifi.md`.

## Generated files

Build directories, generated `sdkconfig`, managed components, dependency locks,
and release archives are local outputs. Keep maintained defaults in
`sdkconfig.defaults` and dependency requirements in `idf_component.yml`.

## Before reporting a result

Record:

- Repository-relative project path.
- Exact ESP-IDF or Arduino core version.
- Resolved BSP and Hosted component versions when relevant.
- Main-board and bottom-board revision.
- Whether the result is compile-only, package verification, or a physical
  hardware test.

Source URLs and hashes are in `docs/sources.md`; hardware limitations are in
`docs/board.md`.
