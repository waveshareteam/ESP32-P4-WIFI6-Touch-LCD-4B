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

- Arduino-ESP32 3.3.11（`esp32:esp32@3.3.11`）
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

在 Arduino-ESP32 3.3.11 中，`USBMode=default` 解析为 USB mode `0`，
`CDCOnBoot=default` 解析为 CDC-on-boot `0`；全局 `Serial` 因此是 UART0，
而不是原生 USB CDC。开发板原理图将 U6 标为调试接口使用的 CH343P USB 转串口桥。
所有首方草图均不等待 `Serial`、DTR 或串口监视器后才启动，因此关闭监视器不得延迟
显示或触摸初始化。

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
repository_root="$(git rev-parse --show-toplevel)"
arduino_data_root="$(arduino-cli config get directories.data)"
arduino_user_root="$(arduino-cli config get directories.user)"
build_temp_root="${TMPDIR:?请将 TMPDIR 设为绝对临时目录}"
prefix_map_flags="-ffile-prefix-map=${repository_root}=REPOSITORY -fmacro-prefix-map=${repository_root}=REPOSITORY -ffile-prefix-map=${arduino_data_root}=ARDUINO_DATA -fmacro-prefix-map=${arduino_data_root}=ARDUINO_DATA -ffile-prefix-map=${arduino_user_root}=ARDUINO_USER -fmacro-prefix-map=${arduino_user_root}=ARDUINO_USER -ffile-prefix-map=${build_temp_root}=BUILD_TEMP -fmacro-prefix-map=${build_temp_root}=BUILD_TEMP"
arduino-cli compile \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=prev3" \
  --build-path build/arduino/HelloWorld \
  --build-property "compiler.c.extra_flags=${prefix_map_flags}" \
  --build-property "compiler.cpp.extra_flags=${prefix_map_flags}" \
  --build-property "compiler.S.extra_flags=${prefix_map_flags}" \
  --library examples/arduino/libraries/Waveshare_ESP32_P4_4B_Display \
  examples/arduino/HelloWorld
```

将 `HelloWorld` 替换为任意首方草图目录。编译只检查源码兼容性；并不能证明显示、触摸、Wi-Fi 或其他硬件行为。
动态 file/macro prefix map 会从每个待打包分段中移除私有工作区、Arduino data/user 与临时目录，
且不会覆盖开发板 `build.extra_flags`（包括 USB/CDC 定义）。

## 分段打包与烧录

构建路径包含 core 生成的 `flash_args` 和 `build.options.json`。发布打包器会核验后者的
精确 FQBN 与 core 3.3.11 路径，但不会将其归档，因为其中包含主机路径。ZIP 改为携带只含
白名单字段的 `metadata/arduino_build_identity.json`，并记录原始来源文件名/大小/SHA-256；
清单声明 canonical 文件自身的大小与哈希。所有偏移、文件名和
安全烧录选项都只从 `flash_args` 推导；生成的 16 MiB 或 32 MiB `*.merged.bin` 绝不作为
发布制品，也不会成为 offset 0 的单文件烧录路径。如果某一目标的真实元数据将 bootloader
放在 offset 0，该合法情况仍会被接受。

使用当前已发布 Registry BSP 的精确证据绑定打包时，请同时记录产品 SHA、BSP 源提交与组件 tree：

```sh
PACKAGE_GIT_SHA="$(git rev-parse HEAD)" python scripts/package_ci_firmware.py arduino \
  --project examples/arduino/HelloWorld \
  --build-dir build/arduino/HelloWorld \
  --framework-version 3.3.11 \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=prev3" \
  --board-profile rev1_3 \
  --bsp-version 3.0.0 \
  --bsp-source-git-sha 32d86900af6916d5e6eb629741d57ee19a577740 \
  --bsp-component-tree-sha 27a67ea44c29375d0309bb5e1dbef25a9c24d6c5 \
  --output release-artifacts/firmware-arduino-HelloWorld-3.3.11-rev1_3.zip
```

ZIP 包含 `metadata/flash_args`、逐段大小与 SHA-256，以及 `flash.sh`/`flash.cmd`。
这些脚本会复现元数据导出的分段 `esptool write_flash` 命令，包括 bootloader、分区表、
仅在实际存在时的 `boot_app0`、应用程序和 core 要求的其他段。解压一个包并检查
`manifest.json` 后，运行以下任一命令：

```sh
sh flash.sh --port PORT
```

```bat
flash.cmd --port PORT
```

不得改用 merged 镜像或擦除/整片烧录命令。
清单中的 `flash.segmented_bytes` 与 `flash.segmented_payload_total` 都等于实际分段大小之和。
发布门禁要求该总数不超过 32 MiB Flash 的一半，以证明没有用名义上的分段包伪装整片写入。
打包器还会扫描每个 Arduino ZIP member 中的私有主机、工作区、cache 与工具路径；如有残留，
必须使用上述 prefix map 重新构建。

## 断开监视器的 HIL 检查

编译、包校验和成功写入都不能证明冷启动。断开或关闭串口监视器后给开发板重新上电，
确认所选草图进入正常的显示/触摸/Wi-Fi 功能；随后再连接 CH343P 调试口并打开 UART0
监视器，确认系统不重启、不卡死且日志符合设计。记录产品 Git SHA、BSP 版本/source/tree
SHA、软件包 SHA-256、开发板版本和结果。此 FQBN 禁用了原生 USB CDC，因此该检查不得
宣称覆盖了原生 CDC 的 HIL。

## Wi-Fi 边界

ESP32-P4 没有集成 Wi-Fi。`GFX_ESPWiFiAnalyzer` 依赖 ESP32-C6 协处理器及其已安装的从机固件。官方示例归档包不包含该固件或其构建说明。请参阅 `docs/p4-c6-hosted-wifi_ZH.md`。

源码和库来源记录在 `docs/sources_ZH.md`；许可边界记录在 `docs/licensing_ZH.md`。
