# Arduino 示例

[English](README.md)

这五个首方草图从官方 ESP32-P4-WIFI6-Touch-LCD-4B 产品示例归档包导入。

| 草图 | 用途 | 额外要求 |
| --- | --- | --- |
| `AsciiTable` | 显示和文本输出演示 | 显示辅助库、GFX 库、PSRAM |
| `Drawing_board` | 触摸绘图界面 | 显示/触摸辅助库、GFX 库、PSRAM |
| `GFX_ESPWiFiAnalyzer` | Wi-Fi 扫描可视化 | 显示辅助库、GFX 库、兼容的 ESP32-C6 固件 |
| `HelloWorld` | 基础显示初始化 | 显示辅助库、GFX 库、PSRAM |
| `LVGLV9_Arduino` | LVGL v9 显示和触摸演示 | 显示/触摸辅助库、GFX 库、LVGL、本地 `lv_conf.h`、PSRAM |

随第三方库提供的示例不属于首方产品草图，因此未纳入产品 CI。

## 支持的工具链

- Arduino-ESP32 core：`3.3.11`
- FQBN：`esp32:esp32:esp32p4`
- GFX Library for Arduino：`1.6.6`
- LVGL：`9.3.0`
- 本地产品辅助库：`libraries/Waveshare_ESP32_P4_4B_Display`

该产品辅助库封装了当前 Arduino-ESP32 版本使用的 ESP-IDF 新版 I2C 主机驱动，并包含开发板专用的显示/触摸设置。

## 开发板选项

编译基线来自维护中的 ESP-IDF 开发板配置：

```text
UploadSpeed=921600
FlashFreq=80
FlashMode=qio
FlashSize=32M
PartitionScheme=app13M_data7M_32MB
PSRAM=enabled
ChipVariant=prev3
USBMode=default
CDCOnBoot=default
MSCOnBoot=default
DFUOnBoot=default
UploadMode=default
DebugLevel=none
EraseFlash=none
JTAGAdapter=default
```

对应的编译 FQBN 为：

```text
esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=prev3
```

`ChipVariant=prev3` 与当前仓库的固件基线相符。在针对不同生产批次修改此选项前，请确认实际芯片版本。

## 安装依赖

当通用库未在本地随仓库提供时，请安装精确的 core 和库版本：

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GFX Library for Arduino@1.6.6"
arduino-cli lib install "lvgl@9.3.0"
```

## 编译

传入仓库库目录，以便找到产品辅助库：

```sh
arduino-cli compile \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=prev3" \
  --libraries examples/arduino/libraries \
  examples/arduino/HelloWorld
```

将 `HelloWorld` 替换为任意首方草图目录。编译只检查源码兼容性；并不能证明显示、触摸、Wi-Fi 或其他硬件行为。

## Wi-Fi 边界

ESP32-P4 没有集成 Wi-Fi。`GFX_ESPWiFiAnalyzer` 依赖 ESP32-C6 协处理器及其已安装的从机固件。官方示例归档包不包含该固件或其构建说明。请参阅 `docs/p4-c6-hosted-wifi_ZH.md`。

源码和库来源记录在 `docs/sources_ZH.md`；许可边界记录在 `docs/licensing_ZH.md`。
