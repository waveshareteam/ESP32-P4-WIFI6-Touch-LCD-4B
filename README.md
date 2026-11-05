<div align="center">
  <h1>ESP32-P4-WIFI6-Touch-LCD-4B</h1>
  <p>ESP32-P4 4-inch 720 × 720 MIPI-DSI touch development board.</p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/build.yml"><img alt="Build" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/releases/latest"><img alt="Latest Release" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/badge/license-Apache--2.0-blue"></a>
  </p>
  <p><strong>English</strong> · <a href="README_ZH.md">简体中文</a></p>
  <p><a href="https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm">🌐 Product</a> &middot; <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/releases/latest">📦 Firmware</a> &middot; <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot; <a href="examples/arduino/examples/">🔧 Arduino</a> &middot; <a href="hardware/">📚 Documentation</a></p>
  <img src="docs/images/esp32-p4-wifi6-touch-lcd-4b.jpg" alt="ESP32-P4-WIFI6-Touch-LCD-4B development board" width="500">
</div>

---

## ✨ Overview

This repository provides ESP-IDF and Arduino examples, factory firmware source, and hardware references for ESP32-P4-WIFI6-Touch-LCD-4B. The factory firmware supports the ST7703 and JD9365 display variants through one application image.

| Item | Specification |
| --- | --- |
| MCU | ESP32-P4 |
| Display | 4-inch 720 × 720 MIPI DSI touch display |
| Flash / PSRAM | 32 MB / 32 MB |
| Wireless | ESP32-C6 coprocessor |

## 📦 Quick Start

1. Download the tested combined factory firmware from [firmware](firmware/README.md).
2. Flash it with the command in that firmware entry.
3. The firmware reports its selected display controller on the serial console during boot.

## 🧪 Examples

| Framework | Examples |
| --- | --- |
| ESP-IDF | [12 projects](examples/esp-idf/README.md) |
| Arduino | [10 sketches](examples/arduino/README.md) |

The open factory project is in [examples/factory](examples/factory/README.md).

## 🛠️ Development Environment

| Framework | Version | Scope |
| --- | --- | --- |
| ESP-IDF | v5.5.3, v5.5.6, v6.1 | First-party examples |
| Arduino-ESP32 | 3.3.12 | Arduino sketches |
| ESP-IDF | v5.5.6 | Factory firmware |

## 📚 Resources

- [Schematics](hardware/schematic/README.md)
- [Mechanical drawing](hardware/mechanical/ESP32-P4-WIFI6-Touch-LCD-4B-Dimensions.pdf)
- [Official product page](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm)
- [Product documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B)

## 🤝 Support

- [GitHub issues](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/issues)
- [Technical support](https://www.waveshare.com/contact_us)

## 📄 License

First-party software and project documentation use [Apache-2.0](LICENSE). Third-party materials retain their own terms; see [licensing boundaries](docs/licensing.md).
