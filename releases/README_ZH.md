# 固件包

[English](README.md) · [简体中文](README_ZH.md)

此目录记录生成的固件包。生成的归档文件不会提交；默认输出位置 `release-artifacts/`、
`releases/dist/` 和 `releases/downloads/` 被忽略。

当前仓库状态没有记录任何发布包生成、烧录或硬件测试。已有被忽略的本机构建目录不是发布证据。

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
./flash.sh
```

在 Windows 上：

```bat
flash.cmd
```

生成的脚本使用 esptool 的默认端口发现行为。当连接多个设备时，只能通过经过审阅的副本或等效
esptool 命令添加显式端口。

Brookesia 软件包必须包含其生成的烧录参数所引用的每个镜像，包括模型和 SPIFFS 镜像。
仅有应用程序二进制文件并不是完整的首次安装软件包。

## 获取 CI 构件

当前工作流仅执行编译验证，不会自动上传固件包。

当工作流上传软件包后，可通过 GitHub Web 界面或 GitHub CLI 下载：

```sh
gh run download RUN_ID --name ARTIFACT_NAME --dir releases/downloads
```

将 CI 构件升级为发布版时，请记录工作流运行、源提交、框架版本和构件 SHA-256。

## Arduino 边界

Arduino 草图目前是编译验证目标。除非 Arduino ZIP 包含与精确 FQBN 和开发板选项相匹配的
正确引导加载程序、分区表、应用程序和偏移量，否则不要将其标记为可烧录。

## 工厂和 C6 固件

CI 构建的软件包不是工厂/恢复镜像。经授权并在以后添加的供应商工厂/恢复构件需要各自的
源代码/版本/哈希记录，且不应由 CI 重建。

ESP32-C6 协处理器固件是独立镜像，且不在为本仓库审阅的官方示例归档中。不要将猜测的 C6
镜像与 P4 软件包一起包含或烧录。参见 `../docs/p4-c6-hosted-wifi_ZH.md`。

## 发布验收

发布软件包前：

1. 使用精确框架版本从干净的源代码版本构建。
2. 将打包文件和偏移量与 `flasher_args.json` 进行比较。
3. 验证所有软件包哈希和两个烧录脚本。
4. 在指定开发板版本上烧录完整软件包。
5. 测试软件包声称支持的外设。
6. 包含 Wi-Fi 时记录 C6 兼容性。
7. 审阅许可、凭据、主机本地路径和用户数据。

仅编译或打包成功并不构成硬件验证。
