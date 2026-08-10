# 官方来源与导入记录

[English](sources.md) · [简体中文](sources_ZH.md)

本仓库对 ESP32-P4-WIFI6-Touch-LCD-4B 的官方 Waveshare 资源材料进行了规范化处理。源归档有意未提交；
下表记录本次导入所用的精确文件。

获取日期：**2026-07-30**

## 产品文档

- [产品文档](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B)
- [资源与文档](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B/Resources-And-Documents)
- [ESP-IDF 开发指南](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B/Development-Environment-Setup-IDF)
- [常见问题](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B/FAQ)

## 已下载资源

| 资源 | 官方 URL | 字节数 | SHA-256 |
| --- | --- | ---: | --- |
| 产品示例 ZIP | [ESP32-P4-WIFI6-Touch-LCD-4B.zip](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/ESP32-P4-WIFI6-Touch-LCD-4B.zip) | 117633366 | `f9b7ec05e1e7d8f955e8af7104cbb8b86f0b2a46c594296449693dff05d6debe` |
| 2D 图纸 ZIP | [ESP32-P4-WIFI6-Touch-LCD-4B-2D.zip](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/ESP32-P4-WIFI6-Touch-LCD-4B-2D.zip) | 132555 | `a7efcb63c2f4ea5fa74144b73a459368b1f3c56b72504a60b7b591650d4282f2` |
| 主板原理图 | [ESP32-P4-WIFI6-Touch-LCD-4B.pdf](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/ESP32-P4-WIFI6-Touch-LCD-4B.pdf) | 3166842 | `24b9b96d06256dfc613d9a555ae34cb4290b119a3830ac40a07846f81b1e70a1` |
| 当前官方 86 面板底板原理图 | [86_Panel_Bottom_Board.pdf](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B/86_Panel_Bottom_Board.pdf) | 476873 | `167957dbba2b13f782cbf8319dc491a0228d0f509d35a6a28152035afb08a44f` |

2D 归档已提取到 `hardware/dimensions/`：

| 仓库文件 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `hardware/dimensions/ESP32-P4-WIFI6-Touch-LCD-4B.dxf` | 997746 | `59c026d9a5ed55ee2466288849a4e898ab87f28d39555470e724d69639adad51` |
| `hardware/dimensions/ESP32-P4-WIFI6-Touch-LCD-4B.pdf` | 26445 | `e690fa9c3c48ec62b1c8167a1d9bd8e9bf44d7b6af4b20f109eb97826c4d1a25` |

`schematic/86 Panel Bottom Board V1.3(1).pdf` 在导入官方资源前已存在。其 SHA-256 为
`23b3dad5b025ac0310a8d17e6716308e71fb2c17d69e082f51df06f23651b86a`，大小为 183700 字节。
本次导入未验证该副本的确切官方下载 URL，因此将其保留为历史 V1.3 参考，而不将其表述为当前下载。

## 示例规范化

| 官方归档路径 | 仓库路径 | 导入处理 |
| --- | --- | --- |
| `ESP-IDF/01_HowToCreateProject` | `examples/esp-idf/project-template` | 规范化名称 |
| `ESP-IDF/02_HelloWorld` | `examples/esp-idf/hello_world` | 仓库维护的等效项目 |
| `ESP-IDF/03_i2c_tools` | `examples/esp-idf/i2c-tools` | 规范化名称 |
| `ESP-IDF/04_sdmmc` | `examples/esp-idf/sd-card` | 规范化名称 |
| `ESP-IDF/05_wifistation` | `examples/esp-idf/wifi-station` | 规范化名称 |
| `ESP-IDF/06_I2SCodec` | `examples/esp-idf/audio-codec` | 规范化名称 |
| `ESP-IDF/07_color_panel` | `examples/esp-idf/lcd-color-test` | 规范化名称 |
| `ESP-IDF/08_lvgl_display_panel` | `examples/esp-idf/display-panel` | 规范化名称 |
| `ESP-IDF/09_lvgl_demo_v8` | `examples/esp-idf/lvgl-v8` | 规范化名称 |
| `ESP-IDF/10_lvgl_demo_v9` | `examples/esp-idf/lvgl-v9` | 规范化名称 |
| `ESP-IDF/11_esp_brookesia_phone` | `firmware/brookesia` | 保留现有维护固件，而不重复归档项目 |
| `ESP-IDF/12_usb_extend_screen` | `examples/esp-idf/usb-extended-screen` | 规范化名称 |
| `ESP-IDF/13_ethernetbasic` | `examples/esp-idf/ethernet` | 规范化名称 |
| `ESP-IDF/14_RS485_Test` | `examples/esp-idf/rs485` | 规范化名称 |
| `Arduino/AsciiTable` | `examples/arduino/AsciiTable` | 导入的第一方草图 |
| `Arduino/Drawing_board` | `examples/arduino/Drawing_board` | 导入的第一方草图 |
| `Arduino/GFX_ESPWiFiAnalyzer` | `examples/arduino/GFX_ESPWiFiAnalyzer` | 导入的第一方草图 |
| `Arduino/HelloWorld` | `examples/arduino/HelloWorld` | 导入的第一方草图 |
| `Arduino/LVGLV9_Arduino` | `examples/arduino/LVGLV9_Arduino` | 导入的第一方草图 |

官方 Arduino 归档捆绑 GFX Library for Arduino 1.6.0 和 LVGL 9.3.0。仓库将 GFX Library for Arduino
固定为 1.6.6，以遵循当前上游 ESP32-P4 时钟分频 API，同时保留 LVGL 9.3.0。本仓库将产品专用的
显示/触摸辅助库保留在 `examples/arduino/libraries/` 下。无论这些大型通用库是随仓库提供还是由构建环境安装，
均不改变仓库矩阵选择的版本。

## 来源与验证

哈希可验证获取了哪些源文件；它们不能证明版权许可、运行时兼容性或硬件正确性。导入的代码正在规范化，
经过仓库维护后可能与 ZIP 不同。再分发边界请参阅 `docs/licensing_ZH.md`，编译验证状态请参阅
`docs/ci_ZH.md`。
