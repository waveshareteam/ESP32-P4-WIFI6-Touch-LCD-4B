# 持续集成

[English](ci.md) · [简体中文](ci_ZH.md)

CI 将首方示例、受维护源码固件和仓库策略划分为明确的独立范围。工作流存在只代表
矩阵契约；只有精确提交上的精确 Actions 作业才能作为编译证据。

## 🧭 范围与边界

- `examples/esp-idf/` 包含 13 个直属首方工程。
- `examples/arduino/` 包含 5 个直属首方草图。随附库以及库自身的示例不属于独立
  产品目标。
- `firmware/brookesia/` 是受维护源码固件，不是示例。它使用独立的源码影响/手动
  作业，绝不由示例 CI 自动发现。
- 文档、治理文件和仓库模板仍会运行可见的策略与路由检查，但不会重编产品固件。

这项划分可防止嵌套的上游演示或固件目录静默扩大产品示例矩阵。

## 📌 版本矩阵

以下稳定版本已于 2026-08-10 重新核验，并使用固定版本而非可移动别名：

| 范围 | 框架版本 | 目标 | 默认条目数 |
| --- | --- | --- | ---: |
| 13 个 ESP-IDF 示例 | [ESP-IDF v5.5.5](https://github.com/espressif/esp-idf/releases/tag/v5.5.5) | `esp32p4` | 13 |
| 13 个 ESP-IDF 示例 | [ESP-IDF v6.0.2](https://github.com/espressif/esp-idf/releases/tag/v6.0.2) | `esp32p4` | 13 |
| 5 个 Arduino 草图 | [Arduino-ESP32 3.3.11](https://github.com/espressif/arduino-esp32/releases/tag/3.3.11) | `esp32:esp32:esp32p4` | 5 |
| Brookesia 固件 | ESP-IDF v5.5.5 | `esp32p4` | 独立工作流 |

因此，影响完整构建范围的变更会生成 26 个默认 ESP-IDF 构建和 5 个 Arduino 编译。
Brookesia 的源码、资源或工作流变更通过独立的 `Firmware Build` 验证，也可手动
选择；该受维护固件的 IDF v6 支持仍处于待办状态，不能从示例矩阵推断。

更新固定版本前，维护者必须重新核验上游稳定版本。默认矩阵不得使用 Beta、RC、
Alpha 或 Preview 版本。

## 🔀 变更文件路由

两个默认构建工作流会在每个拉取请求和每次推送到 `main` 时运行轻量 `discover` 作业。
`scripts/select_ci_targets.py` 读取完整的 Git name-status 范围，包括删除路径和重命名
前后的两个路径，并应用以下规则：

| 变更类别 | ESP-IDF 结果 | Arduino 结果 |
| --- | --- | --- |
| Markdown、原理图、结构图、截图、治理文件、模板 | 不构建 | 不构建 |
| 单个首方示例下的源码或配置 | 该示例的两个固定 IDF 版本 | 该草图 |
| `config/` 共享默认值 | 13 个工程的两个版本 | 不构建 |
| 产品 Arduino 辅助库 | 不构建 | 全部 5 个草图 |
| 框架工作流定义 | 受影响框架的完整矩阵 | 受影响框架的完整矩阵 |
| 选择器、CI 烧录器、打包器或其测试 | 全部 26 个构建 | 全部 5 个编译 |
| `firmware/` 源码、媒体、归档或二进制 | 报告固件已变更；不构建示例 | 报告固件已变更；不构建示例 |
| 未识别的非文档输入 | 全部 26 个构建 | 全部 5 个编译 |

空的变更文件输入属于运行错误，不能作为成功的免构建判定。保守回退规则可在维护者
为新共享文件添加更精确规则之前提供保护。

手动触发接受 `all`、`hello_world` 或 `LVGLV9_Arduino` 之类的目标目录名，或者
精确的仓库相对示例路径。未知输入会失败，不会产生空的绿色运行。Brookesia 必须
通过自己的固件工作流选择，不能通过示例选择器选择。

## 🧱 ESP-IDF 环境

每个矩阵作业在对应固定版本的官方 IDF 容器中运行，加载
`$IDF_PATH/export.sh`，选择 `esp32p4`，在干净检出中解析托管组件，然后构建所选
工程。GitHub 的作业容器包装器会替换镜像入口点，因此必须执行导出步骤；否则
`idf.py` 不会出现在 `PATH` 中。

第三方 Action 固定到已审核的提交 SHA，IDF v5.5.5 和 v6.0.2 容器也固定到本次
迁移所核验的镜像摘要。更新框架版本时必须同步进行明确的摘要审核。

生成的 `managed_components/`、`sdkconfig`、`dependencies.lock` 和 `build/`
目录都不是源码输入。作业不得依赖开发者机器生成的副本。

开发板迁移基线是托管组件 `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.0 BSP。
Hosted Wi-Fi 示例固定使用 `esp_wifi_remote` 1.6.3 和 `esp_hosted` 2.12.11。
主机端编译成功不能证明其与板上 ESP32-C6 镜像兼容。

使用 BSP 3.0.0 的 LVGL 工程将 `esp_lvgl_adapter` 固定为 0.6.3。LVGL 9 工程
使用 9.3.0，因为 adapter 0.6.3 调用了该版本才引入的 API；LVGL 8 工程继续使用
8.4.0，并为 BSP 中 LVGL 9 的旋转类型拼写提供仅用于编译的等价别名。LVGL 9
工程关闭可选的快速内存 IRAM 放置，因为 GCC 15 会拒绝 LVGL 9 重复声明中不同的
自动生成 section 属性。USB 复合设备工程还设置 `USB_DEVICE_UAC_AS_PART`，避免
UAC 组件重复注入由应用维护的 TinyUSB 描述符。

## 🔧 Arduino 环境

Arduino 作业使用：

- Core `esp32:esp32@3.3.11` 和 FQBN `esp32:esp32:esp32p4`。
- `FlashMode=qio`、`FlashSize=32M`、`PSRAM=enabled`、
  `PartitionScheme=app13M_data7M_32MB` 和 `ChipVariant=prev3`。
- 默认 USB、CDC-on-boot 和上传模式，以免独立 CH343P 调试串口被原生 USB CDC
  替代。
- `examples/arduino/libraries/` 下的产品辅助库。
- Arduino Library Manager 中的 GFX Library for Arduino 1.6.6 和 LVGL 9.3.0。

LVGL 草图仅使用公开的 LVGL 核心 API，不依赖上游仓库中未随 Arduino 库发布的
`demos/` 目录。Arduino 编译不能验证 GT911、显示时序或 ESP32-C6 Wi-Fi 运行行为。

## ✅ 策略与证据

`Repository Policy` 运行仓库本地检查器和 Python 标准库单元测试。检查器验证本地
链接与片段、完整的受维护英文/简体中文文档范围、双向语言导航、同语言本地路由、
公开文本隐私和 CI 边界。测试还覆盖预期的 13/5 清单、纯文档变更、直属与共享输入、
固件边界、未知输入回退、重命名、复制、删除、空范围以及精确的手动选择器调用。
合成打包测试会验证完整烧录文件收集、清单哈希与顺序、不安全路径拒绝和覆盖保护；
这些策略检查都不会编译固件。

请统一使用以下证据术语：

| 状态 | 含义 |
| --- | --- |
| 矩阵目标 | 该工程与版本组合计划在 CI 中运行 |
| 编译通过 | 精确提交和框架版本上的 Actions 作业成功完成 |
| 软件包已验证 | 归档清单与构建的烧录参数和文件一致 |
| 硬件已验证 | 已在明确板卡版本上烧录并实际运行指定示例 |

不得将编译成功转化为显示、触摸、音频、存储、USB、C6、以太网、继电器或 RS485
行为声明。被忽略的本地构建树不能作为验证证据。

## 📦 固件软件包

每个成功构建后的矩阵作业都会打包并上传一个保留 14 天的 CI ZIP：
`firmware-esp-idf-<name>-<idf-version>`、`firmware-arduino-<name>-3.3.11` 或
`firmware-brookesia-v5.5.5`。包使用最终 PR SHA（或推送 SHA）；ZIP 不存在时工作流失败。
ESP-IDF 打包从 `flasher_args.json` 获取所有镜像和偏移，因此 Brookesia 也包含模型和文件系统镜像。
Arduino 打包只接受一个合并镜像，或针对已选 32 MiB pre-v3 FQBN 的唯一引导加载程序/分区/OTA/应用布局。

`Flash-CI-Firmware.cmd` 是 Windows 上按顺序进行人工测试的入口。它不会使用过期 SHA 构件、脏或
分离的检出、草稿/缺失 PR 或未验证的软件包。编译/打包成功均不证明已经烧录或通过人工运行测试；
操作员只有在烧录后的测试完成后才标记 PASS。

CI 构建包不等同于工厂或恢复镜像，且绝不包含或烧录 ESP32-C6 协处理器镜像。硬件 PDF、结构图和
导入的资源归档绝不能混入固件制品。生成的软件包继续保存在 `release-artifacts/` 或
`releases/dist/` 等已忽略路径下。

## 🧰 手动触发与复现

默认工作流入口为：

- `ESP-IDF Build`，可选择一个示例。
- `Arduino Build`，可选择一个草图。
- `Firmware Build`，用于明确 Brookesia 边界的源码影响或手动验证。

开发者复现时，应先激活精确框架版本，再运行相同的工程命令：

```sh
idf.py --version
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

Arduino 命令和开发板选项记录在 `examples/arduino/README.md`。记录证据时应使用仓库
相对路径；不得公开机器专属工具路径、用户名、凭据、网络名称或私有设备详情。
