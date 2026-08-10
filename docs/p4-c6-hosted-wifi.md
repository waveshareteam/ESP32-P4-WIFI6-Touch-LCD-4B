# P4 and C6 Hosted Wi-Fi Compatibility

[English](p4-c6-hosted-wifi.md) · [简体中文](p4-c6-hosted-wifi_ZH.md)

ESP32-P4 has no integrated Wi-Fi radio. This board uses an
ESP32-C6-MINI-1U-H8 coprocessor over SDIO. The P4 host software and the C6
slave firmware form one compatibility contract; updating only the P4 side can
produce a successful build with non-functional wireless hardware.

## Host-side baseline

The current ESP-IDF host dependency baseline is:

- `espressif/esp_wifi_remote` 1.6.3.
- `espressif/esp_hosted` 2.12.11.
- SDIO transport and ESP32-C6 as the slave target.

`esp_wifi_remote` 1.6.3 declares an `esp_hosted >=2.11,<3.0` dependency. Do not
pair it with ESP-Hosted 3.x without intentionally migrating the complete host
and slave protocol stack.

## Compatibility matrix

| Repository surface | Host framework | Host dependency | C6 firmware | Compile status | Hardware status |
| --- | --- | --- | --- | --- | --- |
| `examples/esp-idf/wifi-station` | ESP-IDF v5.5.5 | `esp_wifi_remote` 1.6.3, `esp_hosted` 2.12.11 | Not identified in this repository | CI target; consult the workflow result | Not verified |
| `examples/esp-idf/wifi-station` | ESP-IDF v6.0.2 | `esp_wifi_remote` 1.6.3, `esp_hosted` 2.12.11 | Not identified in this repository | CI target; consult the workflow result | Not verified |
| `firmware/brookesia` | ESP-IDF v5.5.5 | Same pinned 2.x host line | Not identified in this repository | CI target; consult the workflow result | Not verified |
| `examples/arduino/GFX_ESPWiFiAnalyzer` | Arduino-ESP32 3.3.11 | Hosted support bundled by the Arduino core; do not assume managed-component versions | Not identified in this repository | Arduino CI target | Not verified |

“CI target” is not a claim that the current commit has passed. A GitHub Actions
run or a recorded local build is required for compile evidence, and a physical
board test is required for runtime evidence.

## Missing C6 material

The official ESP32-P4-WIFI6-Touch-LCD-4B example archive reviewed on
2026-07-30 does not contain:

- A C6 slave firmware binary.
- C6 slave source code.
- C6 build or flashing instructions.
- A version identifier for the image installed at the factory.

These materials are not included in this repository yet and may be added in a
later update. Their absence must not be described as proof that they are
closed, permanently unavailable, or incompatible.

## Safe update procedure

1. Record the current main-board revision and C6 firmware identifier if one can
   be read from the device logs or protocol.
2. Pin the P4 host dependency versions; do not use unconstrained `"*"` ranges.
3. Build the P4 host for each maintained IDF version.
4. Flash the P4 application without replacing the C6 image and verify SDIO
   enumeration, station scan, association, DHCP, sustained traffic, and
   reconnect behavior.
5. If the C6 image is changed, preserve a recovery copy and test it together
   with every maintained P4 host surface.
6. Record the exact working host/slave pair in this matrix.

## Failure interpretation

- Dependency resolution or compilation failure is a P4 host build problem.
- SDIO initialization, RPC, or version-negotiation failure may be a host/slave
  protocol mismatch.
- Successful association without stable traffic can still indicate buffer,
  flow-control, power, or firmware compatibility problems.
- A successful compile never verifies the installed C6 image.

TODO: when official or otherwise authorized C6 images, source, and build
instructions are provided, add their hashes, flash procedure, and verified
host compatibility rows here.
