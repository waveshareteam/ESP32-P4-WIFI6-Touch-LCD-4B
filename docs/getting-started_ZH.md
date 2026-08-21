# 快速开始

[English](getting-started.md) · [简体中文](getting-started_ZH.md)

## 选择框架表面

本仓库包含：

- `examples/esp-idf/` 下的 ESP-IDF 示例。
- `examples/arduino/` 下的 Arduino 草图。
- `firmware/brookesia/` 下较大的 ESP-Brookesia 源代码固件。

在烧录外设示例之前，请先从 `hello_world` 或 Arduino `HelloWorld` 开始。编译可确认工具链兼容性；
只有实体开发板测试才能确认硬件行为。

## 硬件前提条件

- ESP32-P4-WIFI6-Touch-LCD-4B 主板。
- 支持数据传输的 USB 线缆。
- 主机系统可访问的串口。
- 用于以太网、继电器或 RS485 示例的完整匹配 ESP32-P4-86-Panel-ETH-2RO 硬件装配体。
- 用于 Wi-Fi 示例的兼容 ESP32-C6 从机镜像。

在烧录其开发板选项与仓库基线不同的构建之前，请确认实际开发板和 ESP32-P4 芯片版本。

## ESP-IDF 前提条件

维护的示例矩阵使用 ESP-IDF v5.5.5 和 v6.0.2。Brookesia 仅使用 v5.5.5。安装并激活一个明确版本，
然后验证它：

```sh
idf.py --version
```

首次配置/构建需要访问 ESP Component Registry。托管组件会在本地生成，不能直接编辑。

## 构建 ESP-IDF 示例

```sh
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

将 `hello_world` 替换为 `examples/esp-idf/README.md` 中列出的项目。硬件示例可通过 `menuconfig`
公开项目专用设置：

```sh
idf.py -C examples/esp-idf/wifi-station menuconfig
```

不要提交 SSID、密码、令牌、证书、激活标识符或其他凭据。构建失败时，应审查第一个真实的配置/编译器错误，
而不是最后的级联错误。

## 烧录并监视 ESP-IDF 示例

```sh
idf.py -C examples/esp-idf/hello_world -p PORT flash monitor
```

请使用 ESP-IDF 打印的退出序列离开监视器。正常烧录使用项目生成的烧录参数；不要从另一个示例猜测偏移量。

## 构建和烧录 Brookesia

使用 ESP-IDF v5.5.5：

```sh
idf.py --version
idf.py -C firmware/brookesia set-target esp32p4
idf.py -C firmware/brookesia build
idf.py -C firmware/brookesia -p PORT flash monitor
```

首次安装或任何分区/模型/文件系统变更均需要完整 `flash` 操作。`app-flash` 仅更新应用程序，不更新
分区表、ESP-SR 模型镜像或 SPIFFS 存储镜像。请参阅 `firmware/brookesia/README.md` 和
`docs/firmware_ZH.md`。

## Arduino 前提条件

维护的 Arduino 矩阵使用：

- Arduino-ESP32 3.3.11。
- FQBN `esp32:esp32:esp32p4`。
- 随附的 GFX Library for Arduino 1.6.0。
- LVGL 草图使用随附的 LVGL 9.3.0。

安装 Arduino core；完整的 GFX 与 LVGL 库已随附在 `examples/arduino/libraries/` 下，
不得替换：

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

## 编译并上传 Arduino 草图

```sh
arduino-cli compile \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=postv3" \
  --build-path build/arduino/01_HelloWorld \
  --libraries examples/arduino/libraries \
  examples/arduino/examples/01_HelloWorld
```

确认正确端口后：

```sh
arduino-cli upload \
  -p PORT \
  --fqbn "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=postv3" \
  examples/arduino/examples/01_HelloWorld
```

制作发布候选时，不得发布生成的整片镜像。请使用 `examples/arduino/README_ZH.md` 所述的
完整隐私映射编译命令与仓库打包器。该命令会动态映射 repository、Arduino data/user
和临时根目录，并分别传入 C、C++ 与汇编编译，不改变 FQBN。打包器读取实际 core 3.3.11
`flash_args`，生成带哈希的分段清单和
`flash.sh`/`flash.cmd`，绝不猜测偏移。分段烧录后，先在断开 CH343P UART0 监视器时
冷启动，再连接监视器并确认应用不重启、不卡死。编译/打包证据不等同于该 HIL 结果。

Wi-Fi 分析器还依赖于已在 C6 协处理器上运行的兼容镜像。请参阅 `docs/p4-c6-hosted-wifi_ZH.md`。

## 生成文件

构建目录、生成的 `sdkconfig`、托管组件、依赖锁文件和发布归档都是本地输出。将维护的默认值保留在
`sdkconfig.defaults` 中，将依赖要求保留在 `idf_component.yml` 中。

## 报告结果前

请记录：

- 仓库相对项目路径。
- 精确的 ESP-IDF 或 Arduino 核心版本。
- 相关时已解析的 BSP 与 Hosted 组件版本。
- 主板与底板版本。
- 结果是仅编译、包验证还是实体硬件测试。

源 URL 和哈希见 `docs/sources_ZH.md`；硬件限制见 `docs/board_ZH.md`。
