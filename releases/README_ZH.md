# 固件包

[English](README.md) · [简体中文](README_ZH.md)

此目录记录生成的固件包。生成的归档文件不会提交；默认输出位置 `release-artifacts/`、
`releases/dist/` 和 `releases/downloads/` 被忽略。

当前仓库状态没有记录任何发布包生成、烧录或硬件测试。已有被忽略的本机构建目录不是发布证据。

成功的 CI 作业现在会创建保留 14 天的可下载包。它们是可追溯的 CI 构件，不是已发布的工厂或恢复镜像。

## ESP-IDF 包创建

在成功完成 ESP-IDF 构建后，对该构建目录运行仓库辅助工具：

```sh
python scripts/package_esp_idf_firmware.py \
  examples/esp-idf/hello_world/build \
  --output-dir release-artifacts \
  --project hello_world \
  --idf-version v5.5.5
```

有用的可选参数包括：

- 当无法从 CI 环境检测到源代码版本时，使用 `--git-sha` 记录该版本。
- 使用 `--baud` 将默认烧录波特率从 460800 改为其他值。
- 使用 `--overwrite` 显式替换具有相同项目、框架和版本标签的归档文件；默认会拒绝替换。

该辅助工具读取 `flasher_args.json`；它不维护第二份硬编码的偏移表。

## 归档布局

每个 ZIP 包含一个顶层包目录：

```text
<project>-<target>-<idf-version>-<git-sha>/
├── manifest.json
├── flash.py
├── flash.sh
├── flash.cmd
└── bin/
    └── files referenced by flasher_args.json
```

`manifest.json` 记录：

- 包格式版本。
- 项目、仓库相对项目路径，以及可用时的项目版本。
- ESP-IDF 版本和目标芯片。
- 源 Git SHA。
- UTC 创建时间和默认波特率。
- 烧录设置和 esptool 复位选项。
- 每个文件的偏移量、包内路径、大小和 SHA-256。
- 表示完整 `python -m esptool` 命令的参数数组。

ESP-IDF 构建目录下的嵌套路径会保留在 `bin/` 下，因此同名镜像不会互相覆盖。

版本后缀和默认拒绝覆盖的规则可防止两个不同的源代码状态在同一归档名称下悄然互相替换。

## 烧录软件包

烧录前请检查 `manifest.json`。`flash.py` 会验证每个打包文件的安全路径、偏移量、大小和
SHA-256，然后在调用 esptool 前确认已验证的文件列表与命令匹配。主机需要 Python 和
`esptool` 模块。


两个平台包装器都会调用 `flash.py`，后者会将清单参数数组直接传给 Python，而不经过 shell
插值。打包器会拒绝未知的 ESP-IDF 全局 esptool 字段，而不是悄悄丢弃它们。
在 POSIX shell 上：

```sh
sh flash.sh --port PORT
```

在 Windows 上：

```bat
flash.cmd --port PORT
```

生成的脚本只接受一组 `--port PORT`，缺失参数、额外参数或形似选项的参数都会被拒绝。包验证器会从
已验证的元数据计划重新生成两个 helper 并逐字节比较，包括端口解析、写入选项、顺序偏移、带引号的
文件名与命令。

Brookesia 软件包必须包含其生成的烧录参数所引用的每个镜像，包括模型和 SPIFFS 镜像。
仅有应用程序二进制文件并不是完整的首次安装软件包。

## 获取 CI 构件

ESP-IDF、Arduino 和 Brookesia 工作流会在构建成功后打包并上传一个 ZIP，使用
`if-no-files-found: error` 和 14 天保留期。构件名称为
`firmware-esp-idf-<name>-<idf-version>-rev3_x`、`firmware-arduino-<name>-3.3.11-rev3_x`、
`firmware-brookesia-v5.5.5-rev1_3` 和 `firmware-brookesia-v5.5.5-rev3_x`。清单必须包含完整的最终 PR/推送 SHA；它不能作为另一版本的证据。

当工作流上传软件包后，可通过 GitHub Web 界面或 GitHub CLI 下载：

```sh
gh run download RUN_ID --name ARTIFACT_NAME --dir releases/downloads
```

将 CI 构件升级为发布版时，请记录工作流运行、源提交、框架版本和构件 SHA-256。

## 修订 profile

所有示例和 Arduino 软件包仅为 `rev3_x`/post-v3；示例矩阵仍为 26 个 ESP-IDF 构建和
5 个 Arduino 构建，不会翻倍。只有 Brookesia 是提供双 profile 安全构件的受维护产品固件。
`rev1_3` 为 pre-v3（最低 1.00、200 MHz PSRAM 基线）；`rev3_x` 为 post-v3（最低
3.00、250 MHz PSRAM 基线）。不得交叉烧录两个 profile。它们是芯片/配置 profile；现有
主板原理图不足以证明两者之间存在 PCB/电气差异。

## Arduino 边界

Arduino 包要求精确的 32 MiB post-v3 FQBN，并包含 `FlashSize=32M`、`ChipVariant=postv3` 与
`EraseFlash=none`。编译使用 `--build-path`，使 Arduino-ESP32 3.3.11 保留
`build.options.json` 和 `flash_args`。打包器用前者核验精确 FQBN 与 core 路径，但因其包含
主机路径而不归档。工作流/本地命令会对 repository、Arduino data/user 与临时根目录动态设置
C、C++、汇编 prefix map，且不覆盖开发板 flags；随后扫描每个待打包段中的私有路径。白名单
canonical identity 记录原 build-options 文件名/大小/SHA-256，其自身哈希也由清单绑定。安全烧录
选项、偏移和文件名只能来自 `flash_args`。发布 ZIP 包含被引用的
bootloader、分区表、可选 `boot_app0`、应用程序和其他工具链段，绝不发布 16/32 MiB merged
或其他整片主镜像。

每段记录源元数据路径、角色、大小、SHA-256、target、FQBN、框架版本、产品 Git SHA，以及
精确的 BSP 版本/源提交/组件 tree 证据。`metadata/flash_args` 也连同自身大小和哈希入包。
门禁会检查路径安全、符号链接、重复偏移、不重叠、容量、归档字节、元数据一致性，并证明
总分段字节不超过完整 32 MiB Flash 的一半；`segmented_bytes` 与
`segmented_payload_total` 都等于该精确总和。偏移绝不按芯片家族猜测；真实元数据声明 bootloader
位于 offset 0 时仍属于合法情况。

## 跨平台 CI 烧录器

Windows 使用 `Flash-CI-Firmware.cmd`，Linux 使用 `./Flash-CI-Firmware.sh`。两者均转发至同一
Python 核心，需要 Git、带 `esptool` 的 Python，以及已认证的 GitHub CLI 或
`GH_TOKEN`/`GITHUB_TOKEN`。仓库由 `origin` 识别。`--self-test` 为离线测试；`--list` 和
`--preflight` 要求精确本地 HEAD 上完整、成功且未过期的构件，但不会下载或探测硬件。`--preflight`
还会验证本地 `esptool` 导入。Windows 提供等效的 `-SelfTest`、`-List`/`-ListOnly`、
`-Preflight`、`-Item` 与 `-Port` 参数。
每个 workflow 只接受其最新 completed/successful 且 exact-HEAD 的运行；构件集合不完整、过期、为空、
缺失或重复时会 fail closed，不会回退到旧运行。

默认交互模式可自由选择动态推导的任一项，校验一个精确 SHA 的 schema-1 包，探测所选端口，并要求
精确输入 `FLASH` 后才执行一次非擦除 `write_flash`。写入必须出现 `Hash of data verified`；完成
该单项后程序立即退出，绝不会自动前进。profile 检查是芯片/配置检查，不构成 PCB/电气差异的证据。
编译、打包或已验证的写入都不是运行 PASS。

ESP-IDF 包验收还会将每个已验证清单文件的原始元数据路径与包内 `metadata/flasher_args.json` 比对；任何
缺失的引用镜像都会被拒绝。Arduino 验收会将清单、命令和每个已验证段与包内
`metadata/flash_args` 精确比对，保留其中已验证的 Flash 几何选项，并拒绝 merged/整片路径。

## 工厂和 C6 固件

CI 构建的软件包不是工厂/恢复镜像。经授权并在以后添加的供应商工厂/恢复构件需要各自的
源代码/版本/哈希记录，且不应由 CI 重建。

ESP32-C6 协处理器固件是独立镜像，不在任何 P4 CI 软件包中。不要将猜测的 C6 镜像与 P4
软件包一起包含或烧录。参见 `../docs/p4-c6-hosted-wifi_ZH.md`。

## 发布验收

发布软件包前：

1. 使用精确框架版本从干净的源代码版本构建。
2. 将 ESP-IDF 文件与 `flasher_args.json` 比对，或将 Arduino 分段和安全写入选项与
   `flash_args` 比对。
3. 验证所有软件包哈希和两个烧录脚本。
4. 在指定开发板版本上烧录完整软件包。
5. Arduino 必须在断开串口监视器时冷启动；正常进入应用后再连接 CH343P UART 监视器，
   确认系统不重启、不卡死。
6. 测试软件包声称支持的外设。
7. 包含 Wi-Fi 时记录 C6 兼容性。
8. 审阅许可、凭据、主机本地路径和用户数据。

仅编译或打包成功并不构成硬件验证。
