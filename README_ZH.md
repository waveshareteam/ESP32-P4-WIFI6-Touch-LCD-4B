<div align="center">
  <h1>ESP32-P4-WIFI6-Touch-LCD-4B</h1>
  <strong>Waveshare 4 英寸 ESP32-P4 产品的示例源码、开发板资料与受维护固件边界。</strong>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/repository-policy.yml"><img src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/repository-policy.yml/badge.svg" alt="Repository Policy"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/esp-idf.yml"><img src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/esp-idf.yml/badge.svg" alt="ESP-IDF Build"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/arduino.yml"><img src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/arduino.yml/badge.svg" alt="Arduino Build"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/firmware.yml"><img src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/firmware.yml/badge.svg" alt="Firmware Build"></a>
  </p>
  <p><img src="docs/assets/esp32-p4-wifi6-touch-lcd-4b.jpg" width="720" alt="ESP32-P4-WIFI6-Touch-LCD-4B 4 英寸触摸显示屏"></p>
  <p><a href="README.md">English</a> · <strong>简体中文</strong></p>
  <p>
    <a href="https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm">🌐 产品页面</a> ·
    <a href="https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B">📚 产品文档</a> ·
    <a href="docs/firmware_ZH.md">📦 固件</a> ·
    <a href="docs/getting-started_ZH.md">🚀 快速开始</a> ·
    <a href="examples/esp-idf/README_ZH.md">🧩 ESP-IDF</a> ·
    <a href="examples/arduino/README_ZH.md">🔧 Arduino</a>
  </p>
</div>

---

本仓库整理了产品资源包中的首方 ESP-IDF 与 Arduino 示例，并将受维护的
ESP-Brookesia 源码作为独立固件范围保留。新建 ESP-IDF 应用时，显示、触摸、
音频和开发板支持应优先使用已发布的
[`waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.1 BSP](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_4b/versions/3.0.1)。
已发布的 BSP 3.0.1 包含 GT911 地址探测修复。应通过 Component Registry 解析，
不要使用 Git URL 依赖。

## 🖥️ 硬件范围

独立款 ESP32-P4-WIFI6-Touch-LCD-4B 集成 ESP32-P4、32 MB Flash、32 MB
PSRAM、4 英寸 720 × 720 ST7703 双通道 MIPI DSI 屏、GT911 触摸、音频编解码器、
摄像头与 MicroSD 接口、USB，以及 ESP32-C6 无线协处理器。

相关的 ESP32-P4-86-Panel-ETH-2RO 是另一套产品总成。其底板提供 IP101 以太网、
两路继电器和 RS485；相关示例需要完整匹配的总成，不能视为独立 4B 主板的功能。
修改引脚或跨硬件版本复用结论前，请先阅读
[`docs/board_ZH.md`](docs/board_ZH.md) 和
[`schematic/README_ZH.md`](schematic/README_ZH.md)。

ESP-IDF 示例和受维护固件使用的 Registry BSP 3.0.1 将 DSI 通道速率配置为
480 Mbps。产品资源包导入的 Arduino 辅助库同样使用 480 Mbps，但配置一致并不
等同于硬件验证；修改任一实现前，仍需在目标板卡版本上重新验证显示效果。

## 🗂️ 仓库结构

| 路径 | 用途 |
| --- | --- |
| `examples/esp-idf/` | 13 个已整理的首方 ESP-IDF 工程 |
| `examples/arduino/` | 5 个首方 Arduino 草图和 1 个产品辅助库 |
| `firmware/brookesia/` | 受维护的 ESP-Brookesia 源码；不属于示例 CI 目标 |
| `config/` | 共享 ESP-IDF 配置默认值 |
| `docs/` | 开发板、来源、CI、固件与许可说明 |
| `hardware/` | 机械和尺寸资料 |
| `schematic/` | 主板与底板原理图 |
| `releases/` | 固件包格式和获取说明 |
| `.github/` | 持续集成和贡献模板 |

完整示例清单见
[`examples/esp-idf/README_ZH.md`](examples/esp-idf/README_ZH.md) 和
[`examples/arduino/README_ZH.md`](examples/arduino/README_ZH.md)。

## 🧪 框架矩阵

以下默认矩阵已于 2026-08-10 对照上游稳定版本重新核验：

| 范围 | 受维护版本 | 默认 Actions 覆盖 | 运行边界 |
| --- | --- | ---: | --- |
| 13 个 ESP-IDF 示例 | ESP-IDF v5.5.5 与 v6.0.2 | 26 个构建 | 仅提供编译证据 |
| 5 个 Arduino 草图 | Arduino-ESP32 3.3.11 | 5 个编译 | 仅提供编译证据 |
| Brookesia 固件 | ESP-IDF v5.5.5 | 与默认示例 CI 分离 | IDF v6 仍待支持 |

CI 会先分类完整的变更文件范围：单个示例的源码变更只选择受影响工程；共享构建
输入会选择该框架的完整矩阵；纯文档变更保留可见的路由任务，但不会重编固件。
固件变更会被明确报告，绝不会被静默重分类为示例。详细规则见
[`docs/ci_ZH.md`](docs/ci_ZH.md)。

编译成功不等于硬件验证。Wi-Fi 还依赖 ESP32-C6 协处理器上的兼容镜像，而官方
示例资源包并未提供该镜像。参见
[`docs/p4-c6-hosted-wifi_ZH.md`](docs/p4-c6-hosted-wifi_ZH.md)。

## 🚀 快速开始

构建示例前，请先激活明确版本的 ESP-IDF 环境：

```sh
idf.py --version
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

Brookesia 使用独立维护的 IDF v5.5 边界：

```sh
idf.py -C firmware/brookesia set-target esp32p4
idf.py -C firmware/brookesia build
```

Arduino 构建使用 FQBN `esp32:esp32:esp32p4`、32 MB Flash、已启用 PSRAM，
以及 [`examples/arduino/README_ZH.md`](examples/arduino/README_ZH.md) 中列出的选项。
完整构建和烧录说明见 [`docs/getting-started_ZH.md`](docs/getting-started_ZH.md)。

## 📦 来源与固件溯源

官方下载地址、压缩包哈希和规范化目录映射记录在
[`docs/sources_ZH.md`](docs/sources_ZH.md) 中。CI 构建包与未来可能提供的工厂或恢复
镜像属于不同制品类别，参见 [`docs/firmware_ZH.md`](docs/firmware_ZH.md)。生成的构建树、
依赖缓存、锁文件和发布包均有意排除在源码版本控制之外。

## 📄 许可与再分发

本仓库尚未选择仓库级许可证。单个文件或组件可能带有自己的许可证，但这不代表
整个仓库获得相同授权。公开发布或再分发导入的源码、二进制、媒体、原理图或结构图
之前，请阅读 [`docs/licensing_ZH.md`](docs/licensing_ZH.md) 和
[`THIRD_PARTY_NOTICES_ZH.md`](THIRD_PARTY_NOTICES_ZH.md)。仓库支持边界见
[`SUPPORT_ZH.md`](SUPPORT_ZH.md)。
