# 固件与烧录产物

[English](firmware.md) · [简体中文](firmware_ZH.md)

本仓库区分源代码固件、由源代码构建的包以及工厂或恢复镜像。这些术语不能互换。

## 产物类别

| 类别 | 仓库位置 | 含义 |
| --- | --- | --- |
| 源代码固件 | `firmware/brookesia/` | 面向该开发板维护的 ESP-IDF 源代码项目 |
| 由源代码构建的包 | 已忽略的输出，例如 `release-artifacts/` 或 `releases/dist/` | 从特定仓库版本构建的可复现输出 |
| 工厂或恢复镜像 | 当前未包含 | 如后续添加，供生产或恢复使用的厂商提供镜像 |
| ESP32-C6 从机固件 | 当前未包含 | 必须与 P4 Hosted 栈匹配的独立协处理器镜像 |

官方产品压缩包中的 `11_esp_brookesia_phone` 项目由维护的 `firmware/brookesia/` 树表示，
而不是复制为第二个源代码项目。将此源代码称为“工厂固件”并不会使本地构建的二进制文件与厂商烧录的
生产镜像相同。

本仓库尚未包含工厂/恢复镜像以及 C6 源代码/构建说明，后续更新可能会添加。不得将其缺失描述为它们
已关闭或永久不可用的证据。

## Brookesia 源代码固件

Brookesia 项目当前面向 ESP-IDF v5.5.5 与 ESP32-P4。它使用包含应用程序、ESP-SR 模型镜像和
SPIFFS 存储镜像的自定义分区表。完整的首次安装必须使用项目生成的烧录参数，以便将每个必需镜像写入
正确偏移量。

Brookesia 有两个不兼容的开发板 profile：`rev1_3` 是默认/pre-v3 profile（最低芯片版本
1.0、最高排他版本 3.0、200 MHz PSRAM）；`rev3_x` 最低为 3.0，不宣称已经验证的硬件
最高上限，并使用现有 250 MHz PSRAM 设置。CI 为二者使用独立 SDKCONFIG 和 build 目录，
不能共用二进制。v3.x profile 需要 ESP-IDF 5.5.3+ 或 6.0+；v5.5.5 满足软件前提但不能
证明硬件兼容性。还必须确认匹配的 PCB/电气版本。

```sh
idf.py -C firmware/brookesia -p PORT flash monitor
```

仅当分区表、模型配置和 SPIFFS 资源均未变更时才使用 `app-flash`。它不会更新 `srmodels.bin`、
`storage.bin` 或分区表。

Brookesia 不声称支持 IDF v6。P4/C6 无线兼容性约定另见 `p4-c6-hosted-wifi_ZH.md`。

## 由源代码构建的包

`scripts/package_esp_idf_firmware.py` 会打包 ESP-IDF 构建目录 `flasher_args.json` 引用的每个镜像。
它创建的 ZIP 包含：

- `manifest.json`，其中包含目标、框架版本、仓库相对项目路径、源版本、时间戳、烧录设置、偏移量、
  大小和 SHA-256 哈希。
- `flash.py`，它在无需 shell 的情况下执行清单参数数组前，验证包文件及命令映射。
- `flash.sh` 和 `flash.cmd`，它们进入包目录并调用 Python 运行器，不在 shell 命令中嵌入镜像名称。
- `bin/`，其中包含每个被引用的镜像，并在需要时保留嵌套构建路径。

示例，在成功构建后：

```sh
python scripts/package_esp_idf_firmware.py \
  examples/esp-idf/hello_world/build \
  --output-dir release-artifacts \
  --project hello_world \
  --idf-version v5.5.5
```

当接收者运行随附的烧录脚本时，该辅助工具要求 Python `esptool` 模块。详细信息见
`../releases/README.md`。

包目录名称包含简短的源版本标签。除非提供 `--overwrite`，否则不会替换现有归档；未知的
`extra_esptool_args` 字段会使打包停止，而不是生成不完整的烧录命令。

Arduino CI 包只有在精确 ESP32-P4 FQBN、一个可接受的二进制布局、每个偏移量和每个哈希均通过精确
SHA 的 Actions 包检查后才可烧录。仅编译本身不是软件包、烧录或运行验证结果。

## CI 固件包与 Windows 烧录

每个成功的 CI 构建都会生成一个 schema-1 ZIP 包。该包记录完整源 SHA、开发板
`ESP32-P4-WIFI6-Touch-LCD-4B`、`esp32p4`、明确的 `rev1_3` 或 `rev3_x` 开发板 profile
及可审计的芯片版本范围、32 MiB Flash 上限、
源工程、偏移量、大小与 SHA-256。包中不包含 ESP32-C6 协处理器镜像，Windows 烧录器也会拒绝
声明包含该镜像的清单。

在 Windows 上，请从干净且未分离 HEAD 的检出目录启动 `Flash-CI-Firmware.cmd`。它要求 GitHub
CLI 已认证、Python 具备 `esptool`、恰好一个指向完整本地 HEAD 的已就绪非草稿拉取请求，以及该
SHA 上成功的工作流。它只下载匹配的 CI 构件，绝不擦除 Flash；每次写入前探测所选 P4，验证清单，
并要求出现 `Hash of data verified`。自动发现时必须恰好存在一个名称明确为 CH343 或 ESP32-P4 的
串口设备；否则传入 `-Port COMx`。

`-ListOnly` 会按固定顺序列出全部 33 项供审计：26 个 ESP-IDF 示例/版本组合、5 个 Arduino
草图，以及两个 Brookesia profile。正常 GUI 使用时会先探测 P4 profile，只显示和处理 32 项
`rev1_3` 或 1 项 `rev3_x`。每个 profile 的进度均隔离在独立的
`state-v4-<profile>.json` 文件中，且仅当最终 SHA 与状态 schema 版本相同时才恢复。芯片 major
revision 低于 3 会选择 `rev1_3`，3 或更高会选择 `rev3_x`。v3.x 芯片探测不代表 PCB/电气
版本兼容。每次写入验证后，操作员必须完成对应运行测试并明确标记 PASS，才会烧录下一项。

## 工厂与恢复镜像

如果日后添加经授权的厂商镜像：

- 将其与源代码项目和 CI 输出分开存放。
- 记录产品、主板版本、相关时的底板版本、ESP32-P4 芯片版本、C6 固件关系、Flash 大小、偏移量、
  版本、源 URL、获取日期和 SHA-256。
- 包含厂商的烧录与恢复说明，不要凭记忆改写偏移量。
- 不要像对待源代码构建产物一样在 CI 中重新构建或重新打包它们。
- 保留其原始许可和再分发条款。

## 验证状态

此次源码变更及其静态检查本身不能证明 Actions 已生成软件包、软件包已经烧录，或硬件已正确运行。
这些声明应使用精确已提交 SHA 的 Actions 证据和已命名开发板的测试记录。已忽略的探索性构建输出不是发布证据。

发布记录应区分：

1. 源提交和精确工具链版本。
2. 成功编译证据。
3. 相对于 `flasher_args.json` 的包内容验证。
4. 在指定硬件版本上的烧录和运行结果。

## 敏感数据

分发包之前，请确认其中不含 Wi-Fi 凭据、API 密钥、证书、激活标识符、用户数据或主机本地路径。
生成的清单必须使用仓库相对项目路径和通用命令。
