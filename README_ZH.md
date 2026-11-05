<div align="center">
  <h1>ESP32-P4-WIFI6-Touch-LCD-4B</h1>
  <p>基于 ESP32-P4 的 4 英寸 720 × 720 MIPI-DSI 触摸开发板。</p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/build.yml"><img alt="构建" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/releases/latest"><img alt="最新版本" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B"></a>
    <a href="LICENSE"><img alt="许可证" src="https://img.shields.io/badge/license-Apache--2.0-blue"></a>
  </p>
  <p><a href="README.md">English</a> · <strong>简体中文</strong></p>
  <p><a href="https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm">🌐 产品</a> &middot; <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/releases/latest">📦 固件</a> &middot; <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot; <a href="examples/arduino/examples/">🔧 Arduino</a> &middot; <a href="hardware/">📚 文档</a></p>
  <img src="docs/images/esp32-p4-wifi6-touch-lcd-4b.jpg" alt="ESP32-P4-WIFI6-Touch-LCD-4B 开发板" width="500">
</div>

---

## ✨ 概述

本仓库提供 ESP32-P4-WIFI6-Touch-LCD-4B 的 ESP-IDF 和 Arduino 示例、出厂固件源码及硬件资料。出厂固件使用同一应用镜像支持 ST7703 与 JD9365 两种显示屏。

| 项目 | 规格 |
| --- | --- |
| MCU | ESP32-P4 |
| 显示屏 | 4 英寸 720 × 720 MIPI DSI 触摸屏 |
| Flash / PSRAM | 32 MB / 32 MB |
| 无线 | ESP32-C6 协处理器 |

## 📦 快速开始

1. 从 [firmware](firmware/README_ZH.md) 下载经过实测的合并出厂固件。
2. 按该条目中的命令烧录。
3. 固件启动时会在串口输出所选的显示控制器。

## 🧪 示例

**ESP-IDF（12 个工程）**

| 示例 | 用途 |
| --- | --- |
| [project-template](examples/esp-idf/project-template/README_ZH.md) | 最小项目结构 |
| [hello_world](examples/esp-idf/hello_world/README_ZH.md) | 最小应用和 CI 冒烟测试 |
| [i2c-tools](examples/esp-idf/i2c-tools/README_ZH.md) | 交互式 I2C 扫描和寄存器访问 |
| [sd-card](examples/esp-idf/sd-card/README_ZH.md) | MicroSD 卡访问 |
| [wifi-station](examples/esp-idf/wifi-station/README_ZH.md) | 通过 ESP32-C6 的站点模式 Wi-Fi |
| [audio-codec](examples/esp-idf/audio-codec/README_ZH.md) | ES8311 播放/音频路径 |
| [lcd-color-test](examples/esp-idf/lcd-color-test/README_ZH.md) | 显示屏色彩测试 |
| [display-panel](examples/esp-idf/display-panel/README_ZH.md) | 基于 BSP 的显示/触摸 UI |
| [lvgl-v9](examples/esp-idf/lvgl-v9/README_ZH.md) | LVGL 9 演示 |
| [usb-extended-screen](examples/esp-idf/usb-extended-screen/README_ZH.md) | USB 显示/音频/触摸组合 |
| [ethernet](examples/esp-idf/ethernet/README_ZH.md) | IP101 以太网 |
| [rs485](examples/esp-idf/rs485/README_ZH.md) | UART/RS485 收发 |

**Arduino（10 个草图）**

| 示例 | 用途 |
| --- | --- |
| [01_HelloWorld](examples/arduino/examples/01_HelloWorld/README_ZH.md) | Arduino_GFX DSI 显示最小点亮 |
| [02_AsciiTable](examples/arduino/examples/02_AsciiTable/README_ZH.md) | Arduino_GFX 能力/基准表 |
| [03_Drawing_board](examples/arduino/examples/03_Drawing_board/README_ZH.md) | GT911 五点电容触摸画板 |
| [04_LVGLV9_Arduino](examples/arduino/examples/04_LVGLV9_Arduino/README_ZH.md) | LVGL 9 控件界面 + 触摸 |
| [05_GFX_ESPWiFiAnalyzer](examples/arduino/examples/05_GFX_ESPWiFiAnalyzer/README_ZH.md) | 图形化 Wi-Fi 扫描(板载 ESP32-C6 协处理器) |
| [06_Camera_Preview](examples/arduino/examples/06_Camera_Preview/README_ZH.md) | OV5647 MIPI-CSI 摄像头实时上屏 |
| [07_Camera_ISP_Tuning](examples/arduino/examples/07_Camera_ISP_Tuning/README_ZH.md) | 实时预览 + 串口交互 ISP/3A 调参 |
| [08_SD_Card](examples/arduino/examples/08_SD_Card/README_ZH.md) | microSD SDIO 3.0 读写 |
| [09_Audio_Playback](examples/arduino/examples/09_Audio_Playback/README_ZH.md) | ES8311 编解码器以不同频率音色演奏《致爱丽丝》开篇 |
| [10_Mic_Record](examples/arduino/examples/10_Mic_Record/README_ZH.md) | ES7210 四麦克风采集,串口打印峰值/RMS/抽样数据 |

开放的出厂工程位于 [examples/factory](examples/factory/README_ZH.md)。

## 🛠️ 开发环境

| 框架 | 版本 | 范围 |
| --- | --- | --- |
| ESP-IDF | v5.5.3、v5.5.6、v6.1 | 首方示例 |
| Arduino-ESP32 | 3.3.12 | Arduino 草图 |
| ESP-IDF | v5.5.6 | 出厂固件 |

## 📚 资料

- [原理图](hardware/schematic/README_ZH.md)
- [尺寸图](hardware/mechanical/ESP32-P4-WIFI6-Touch-LCD-4B-Dimensions.pdf)
- [产品页面](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm)
- [产品文档](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-4B/)

## 🤝 支持

- [技术支持](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-4B/Technical-Support/)

## 📄 许可证

第一方软件和项目文档采用 [Apache-2.0](LICENSE)。第三方材料保留各自条款，详见[许可边界](docs/licensing_ZH.md)。
