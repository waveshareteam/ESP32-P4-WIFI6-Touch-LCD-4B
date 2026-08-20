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

- Arduino-ESP32 3.3.11 (`esp32:esp32@3.3.11`)
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
ChipVariant=postv3
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
esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=postv3
```

`ChipVariant=postv3` matches the default `rev3_x` example profile. It selects
ESP32-P4 v3.00 or newer; confirm the actual silicon revision before changing
the option. The retained `rev1_3` Brookesia profile is an opt-in pre-v3 profile
and is not the example default.

With Arduino-ESP32 3.3.11, `USBMode=default` resolves to USB mode `0` and
`CDCOnBoot=default` resolves to CDC-on-boot `0`; global `Serial` is therefore
UART0, not native USB CDC. The board schematic labels U6 as the CH343P
USB-to-UART bridge for the debug connector. None of the first-party sketches
waits for `Serial`, DTR, or a monitor before starting, so leaving the monitor
closed must not delay display or touch initialization.

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
repository_root="$(git rev-parse --show-toplevel)"
arduino_data_root="$(arduino-cli config get directories.data)"
arduino_user_root="$(arduino-cli config get directories.user)"
build_temp_root="${TMPDIR:?export TMPDIR as an absolute temporary directory}"
prefix_map_flags="-ffile-prefix-map=${repository_root}=REPOSITORY -fmacro-prefix-map=${repository_root}=REPOSITORY -ffile-prefix-map=${arduino_data_root}=ARDUINO_DATA -fmacro-prefix-map=${arduino_data_root}=ARDUINO_DATA -ffile-prefix-map=${arduino_user_root}=ARDUINO_USER -fmacro-prefix-map=${arduino_user_root}=ARDUINO_USER -ffile-prefix-map=${build_temp_root}=BUILD_TEMP -fmacro-prefix-map=${build_temp_root}=BUILD_TEMP"
arduino-cli compile \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=postv3" \
  --build-path build/arduino/HelloWorld \
  --build-property "compiler.c.extra_flags=${prefix_map_flags}" \
  --build-property "compiler.cpp.extra_flags=${prefix_map_flags}" \
  --build-property "compiler.S.extra_flags=${prefix_map_flags}" \
  --library examples/arduino/libraries/Waveshare_ESP32_P4_4B_Display \
  examples/arduino/HelloWorld
```

Replace `HelloWorld` with any first-party sketch directory. Compilation checks
source compatibility; it does not prove display, touch, Wi-Fi, or other
hardware behavior. The dynamic file/macro prefix maps remove private workspace,
Arduino data/user, and temporary roots from every packaged segment without
overriding the board's `build.extra_flags` (including its USB/CDC defines).

## Segmented package and flash

The build path contains core-generated `flash_args` and `build.options.json`.
The release packager verifies the latter's exact FQBN and core 3.3.11 path but
does not archive it because it contains host paths. Instead, the ZIP carries a
canonical, whitelisted `metadata/arduino_build_identity.json` plus raw source
filename/size/SHA-256 evidence; its size and hash are declared in the manifest.
The packager derives every offset,
filename, and safe flash option only from `flash_args`; a generated 16 MiB or
32 MiB `*.merged.bin` is never a release artifact or a single-file offset-zero
flash path. A bootloader at offset zero remains valid on a target whose real
metadata says so.

For the currently published Registry BSP evidence binding, package the build
with the exact product SHA, BSP source commit, and component tree:

```sh
PACKAGE_GIT_SHA="$(git rev-parse HEAD)" python scripts/package_ci_firmware.py arduino \
  --project examples/arduino/HelloWorld \
  --build-dir build/arduino/HelloWorld \
  --framework-version 3.3.11 \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=postv3" \
  --board-profile rev3_x \
  --bsp-version 3.0.0 \
  --bsp-source-git-sha 32d86900af6916d5e6eb629741d57ee19a577740 \
  --bsp-component-tree-sha 27a67ea44c29375d0309bb5e1dbef25a9c24d6c5 \
  --output release-artifacts/firmware-arduino-HelloWorld-3.3.11-rev3_x.zip
```

The ZIP contains `metadata/flash_args`, per-segment sizes and SHA-256 values,
and `flash.sh`/`flash.cmd`. Those scripts reproduce the metadata-derived
segmented `esptool write_flash` command, including bootloader, partition table,
`boot_app0` only when present, application, and any other core-required segment.
Extract one package, inspect `manifest.json`, then run one of:

```sh
sh flash.sh --port PORT
```

```bat
flash.cmd --port PORT
```

Do not replace this with a merged image or an erase/whole-flash command.
The manifest fields `flash.segmented_bytes` and
`flash.segmented_payload_total` both equal the sum of actual segment sizes. The
release gate requires this total to be no more than half of the 32 MiB flash,
which proves that a nominal whole-flash write is not being disguised as a
segmented package. Every Arduino ZIP member is also scanned for private host,
workspace, cache, and tool paths; rebuild with the prefix maps if any remain.

## Monitor-disconnected HIL check

Compilation, package validation, and a successful write do not prove cold
startup. Disconnect or close the serial monitor, power-cycle the board, and
confirm that the selected sketch reaches normal display/touch/Wi-Fi behavior.
Only then connect the CH343P debug port and open the UART0 monitor; confirm that
the board neither resets nor hangs and that logs appear as designed. Record the
product Git SHA, BSP version/source/tree SHAs, package SHA-256, board revision,
and result. Native USB CDC is disabled by this FQBN, so this check must not be
reported as native-CDC HIL coverage.

## Wi-Fi boundary

ESP32-P4 has no integrated Wi-Fi. `GFX_ESPWiFiAnalyzer` depends on the
ESP32-C6 coprocessor and its installed slave firmware. The official example
archive does not include that firmware or its build instructions. See
`docs/p4-c6-hosted-wifi.md`.

Source and library provenance is recorded in `docs/sources.md`; licensing
boundaries are recorded in `docs/licensing.md`.
