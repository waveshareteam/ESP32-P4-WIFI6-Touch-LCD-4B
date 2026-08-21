# 固件与烧录产物

[English](firmware.md) · [简体中文](firmware_ZH.md)

本仓库区分源代码固件、由源代码构建的包以及工厂或恢复镜像。这些术语不能互换。

## 产物类别

| 类别 | 仓库位置 | 含义 |
| --- | --- | --- |
| 源代码固件 | `firmware/brookesia/` | 面向该开发板维护的 ESP-IDF 源代码项目 |
| 由源代码构建的包 | 已忽略的输出，例如 `release-artifacts/` 或 `releases/dist/` | 从特定仓库版本构建的可复现输出 |
| 已提交的默认源码构建镜像 | `firmware/ESP32-P4-WIFI6-Touch-LCD-4B-Brookesia-rev3_x-260821.bin` | 适用于 ESP32-P4 rev3.x 芯片的完整 32 MiB Brookesia 镜像 |
| 工厂或恢复镜像 | 当前未包含 | 如后续添加，供生产或恢复使用的厂商提供镜像 |
| ESP32-C6 从机固件 | 当前未包含 | 必须与 P4 Hosted 栈匹配的独立协处理器镜像 |

官方产品压缩包中的 `11_esp_brookesia_phone` 项目由维护的 `firmware/brookesia/` 树表示，
而不是复制为第二个源代码项目。将此源代码称为“工厂固件”并不会使本地构建的二进制文件与厂商烧录的
生产镜像相同。

本仓库尚未包含工厂/恢复镜像以及 C6 源代码/构建说明，后续更新可能会添加。不得将其缺失描述为它们
已关闭或永久不可用的证据。

## 已提交的默认源码构建镜像

[`ESP32-P4-WIFI6-Touch-LCD-4B-Brookesia-rev3_x-260821.bin`](../firmware/ESP32-P4-WIFI6-Touch-LCD-4B-Brookesia-rev3_x-260821.bin)
是面向 ESP32-P4 芯片版本 v3.00 或更高版本的默认已提交镜像。它是由源码构建的 Brookesia 镜像，
不是厂商工厂/恢复镜像，也不是 CI 生成的分段 ZIP 构件。该镜像由源提交
`f417f6b764f06dddb89fd4f30730ecf4b1fc56d3` 使用 ESP-IDF v5.5.5 和
`firmware/brookesia/sdkconfig.defaults.rev3_x` profile（250 MHz PSRAM）构建。

该文件是 32 MiB 的整片 Flash 原始镜像。其布局来自构建输出的 `flasher_args.json`：bootloader 位于
`0x2000`，分区表位于 `0x8000`，OTA 数据位于 `0x10d000`，ESP-SR 模型位于 `0x110000`，
应用程序位于 `0x200000`，存储位于 `0xa00000`。仅在需要替换完整 32 MiB Flash 内容时才从
Flash 偏移 `0x0` 写入。该操作会覆盖现有分区、应用数据和存储；请先备份需要保留的数据。

镜像不包含 ESP32-C6 协处理器固件。Hosted Wi-Fi 运行仍需要与之兼容的 C6 固件/运行时组合，
详见 [P4/C6 Hosted Wi-Fi](p4-c6-hosted-wifi_ZH.md)。该镜像仅完成源码构建和分段布局检查；
本仓库尚未对它执行硬件烧录、显示、触摸、音频、摄像头、SD 或 Wi-Fi 的 HIL 验证。

`rev1_3` 仍作为面向 pre-v3 芯片的独立分段 CI 构建 profile 保留。它不会添加第二个已提交的
默认整片镜像，且两个 profile 不能交叉使用。

## Brookesia 源代码固件

Brookesia 项目当前面向 ESP-IDF v5.5.5 与 ESP32-P4。它使用包含应用程序、ESP-SR 模型镜像和
SPIFFS 存储镜像的自定义分区表。完整的首次安装必须使用项目生成的烧录参数，以便将每个必需镜像写入
正确偏移量。

Brookesia 有两个不兼容的芯片/配置 profile：`rev1_3` 是默认 pre-v3 profile（最低芯片版本
1.00、最高排他版本 3.00、200 MHz PSRAM）；`rev3_x` 是 post-v3 profile（最低 3.00、
250 MHz PSRAM）。CI 为二者使用独立 SDKCONFIG 和 build 目录，不能共用二进制。v3.x
profile 需要 ESP-IDF 5.5.3+ 或 6.0+；v5.5.5 满足软件前提但不能证明硬件兼容性。现有主板
原理图不足以证明这些 profile 名称之间存在 PCB/电气差异。

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

Arduino CI 包只有在实际构建的 `build.options.json` 证明精确 ESP32-P4 FQBN 与 core 3.3.11，
且每个安全选项、分段、偏移、大小和哈希均与 core 生成的 `flash_args` 一致并通过精确 SHA 的
Actions 包检查后才可烧录。含主机路径的 build options 文件不会归档。仅编译本身不是软件包、
烧录或运行验证结果。

## CI 固件包与跨平台烧录

每个成功的 CI 构建都会生成一个 schema-1 ZIP 包。该包记录完整源 SHA、开发板
`ESP32-P4-WIFI6-Touch-LCD-4B`、`esp32p4`、明确的 `rev1_3` 或 `rev3_x` 开发板 profile
及可审计的芯片版本范围、32 MiB Flash 上限、
源工程、偏移量、大小与 SHA-256。包中不包含 ESP32-C6 协处理器镜像，Windows 烧录器也会拒绝
声明包含该镜像的清单。

请从干净、未分离 HEAD 的检出目录使用仓库顶层包装器。Windows 使用
`Flash-CI-Firmware.cmd`（经 PowerShell 转发），Linux 使用 `./Flash-CI-Firmware.sh`。需要 Git、
带 `esptool` 的 Python，以及已认证的 GitHub CLI 或 `GH_TOKEN`/`GITHUB_TOKEN`。仓库 owner/name
从 `origin` 自动识别，不会复制到命令中。

```text
Flash-CI-Firmware.cmd -SelfTest
Flash-CI-Firmware.cmd -List
Flash-CI-Firmware.cmd -Preflight
Flash-CI-Firmware.cmd -Item 1 -Port COMx
./Flash-CI-Firmware.sh --self-test
./Flash-CI-Firmware.sh --list
./Flash-CI-Firmware.sh --preflight
./Flash-CI-Firmware.sh --item 1 --port /dev/ttyUSB0
```

`SelfTest` 为离线测试。`List` 会认证并只列出当前分支精确本地 HEAD 对应的完整成功工作流产物。
`Preflight` 会检查 Git/origin/认证、Python `esptool`、精确 HEAD 运行记录及未过期且非空的构件元数据，
不会下载构件，也不会打开串口。两种模式都会拒绝不完整运行或旧 SHA 的运行。
对于每个 workflow，最新的 completed/successful 且 exact-HEAD 的运行是唯一候选；如果其构件集合不完整、
过期、为空、缺失或重复，命令会失败，绝不会回退到较旧运行。

普通使用先执行同样的预检，随后可以自由选择动态推导出的 33 个产物（26 个 ESP-IDF、5 个 Arduino、
2 个 Brookesia profile）。程序下载到操作系统用户缓存，并验证 schema-1 清单、路径、哈希、偏移量、
容量及规范的非擦除命令，再探测所选端口。芯片 major revision 小于 3 时必须使用 `rev1_3`，3 或更高
时必须使用 `rev3_x`；这是芯片/配置选择，不构成 PCB/电气差异的证据。操作员必须精确输入 `FLASH`；单项写入必须输出
`Hash of data verified` 后程序即退出，绝不会自动前进。

对于 ESP-IDF 包，每个通过验证的清单文件还带有其原始 `flasher_args.json` 元数据路径。烧录器要求该
映射与包内的 `metadata/flasher_args.json` 完全一致，因此缺少模型、存储、bootloader、分区表或应用程序
镜像均会被拒绝。Arduino 包会先用实际构建的 `build.options.json` 证明精确 ESP32-P4 FQBN 与
core 3.3.11，但不归档这个含主机路径的文件。包内 `metadata/flash_args` 以及其实际引用的
bootloader、分区表、可选 `boot_app0`、应用程序和其他段必须完全匹配。烧录器会验证元数据/段哈希、
安全路径、不重叠、容量、有效字节总数、产品 SHA、FQBN、target 与 BSP 版本/source/tree 绑定，
并保留元数据导出的写入选项和偏移。merged/整片单文件路径会被拒绝；只有真实元数据如此声明时，
bootloader 才可合法位于 offset 0。白名单 canonical identity 会携带原 build-options 文件名/大小/
SHA-256 证据；动态 C/C++/汇编 file/macro prefix map 会移除构建根目录，且打包会扫描全部 Arduino
member 中残留的私有主机路径。`segmented_bytes` 与 `segmented_payload_total` 均等于段大小总和，
且不得超过 32 MiB Flash 的一半。
ZIP 验证器还会从该已验证计划逐字节重新生成 `flash.sh` 和 `flash.cmd`。每个 helper 只接受
`--port PORT`，拒绝其他参数形式，并保留全部选项及有序的 offset/file 对。

Arduino 运行验收还要求独立的冷启动 HIL：关闭并断开监视器后重新上电，确认应用正常启动；随后连接
开发板 CH343P UART0 监视器，确认不重启、不卡死且日志符合预期。固定 Arduino-ESP32 3.3.11 FQBN
把 `USBMode=default`/`CDCOnBoot=default` 解析为禁用原生 CDC，因此该检查不能宣称覆盖原生 USB CDC。

本地 `package_esp_idf_firmware.py` 的包格式及历史本地打包示例与以上 CI schema-1 构件契约相互独立。

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
