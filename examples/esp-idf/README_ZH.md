# ESP-IDF 示例

[English](README.md)

此目录包含从官方 ESP32-P4-WIFI6-Touch-LCD-4B 产品示例归档包规范化而来的首方 ESP-IDF 项目。项目均为本目录的直接子目录，以便构建发现不会意外包含组件内嵌套的测试或示例。

| 项目 | 官方来源 | 用途 | 硬件或运行时要求 |
| --- | --- | --- | --- |
| [project-template](project-template/README_ZH.md) | `01_HowToCreateProject` | 最小项目结构 | 除 ESP32-P4 外无要求 |
| [hello_world](hello_world/README_ZH.md) | `02_HelloWorld` 等效项目 | 最小应用和 CI 冒烟测试 | 除 ESP32-P4 外无要求 |
| [i2c-tools](i2c-tools/README_ZH.md) | `03_i2c_tools` | 交互式 I2C 扫描和寄存器访问 | 谨慎使用文档说明的共享 I2C 引脚 |
| [sd-card](sd-card/README_ZH.md) | `04_sdmmc` | MicroSD 卡访问 | 主板 MicroSD 卡槽 |
| [wifi-station](wifi-station/README_ZH.md) | `05_wifistation` | 通过 ESP32-C6 的站点模式 Wi-Fi | SSID/密码和兼容的 C6 固件 |
| [audio-codec](audio-codec/README_ZH.md) | `06_I2SCodec` | ES8311 播放/音频路径 | 主板音频硬件 |
| [lcd-color-test](lcd-color-test/README_ZH.md) | `07_color_panel` | ST7703 面板色彩测试 | 主板显示屏 |
| [display-panel](display-panel/README_ZH.md) | `08_lvgl_display_panel` | 基于 BSP 的显示/触摸 UI | 主板显示屏和触摸 |
| [lvgl-v9](lvgl-v9/README_ZH.md) | `10_lvgl_demo_v9` | LVGL 9 演示 | 主板显示屏和触摸 |
| [usb-extended-screen](usb-extended-screen/README_ZH.md) | `12_usb_extend_screen` | USB 显示/音频/触摸组合 | USB 主机和设备配置 |
| [ethernet](ethernet/README_ZH.md) | `13_ethernetbasic` | IP101 以太网 | 匹配的 ESP32-P4-86-Panel-ETH-2RO 组件 |
| [rs485](rs485/README_ZH.md) | `14_RS485_Test` | UART/RS485 收发 | 匹配的 ESP32-P4-86-Panel-ETH-2RO 组件；见下文版本说明 |

官方 `11_esp_brookesia_phone` 项目由维护中的 `examples/factory/` 源码树表示，未在此重复。

## 构建矩阵

支持的框架版本以[产品配置](../../.github/product.json)为准。开放的出厂源码单独维护在 [examples/factory](../factory/README_ZH.md)。

默认示例使用 post-v3 芯片配置；内存时钟由 Board Manager 的板定义提供。出厂工程默认用于 rev1.x 芯片（1.00–1.99、200 MHz PSRAM）。芯片配置与 PCB 电气版本分别管理。

显式激活所需 IDF 版本，然后构建一个示例：

```sh
python -m pip install --upgrade esp-bmgr-assist
idf.py -C examples/esp-idf/hello_world bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

对于硬件示例，将 `hello_world` 替换为项目名称。除非已缓存，受管组件会在首次配置/构建时下载。请勿编辑生成的 `managed_components/`、`dependencies.lock`、`sdkconfig` 或 `build/` 输出。

## 开发板支持

显示、触摸和音频通过 Board Manager 或产品 BSP 兼容层使用共享的 `waveshare/waveshare_boards` 板定义，
由板定义选择匹配的屏幕初始化和时序。

## 硬件限制

- `wifi-station` 需要与 P4 主机栈兼容的 C6 从机镜像。官方产品示例归档包未包含该从机镜像的源码或二进制文件。
- `ethernet` 和 `rs485` 使用相关 86-panel 组件中的电路，而非独立 4B 板。请勿假定其底板可直接用于 4B 主 PCB。
- 经审查的历史 V1.3 底板原理图将 RS485 `/RE` 和 `DE` 连接在一起并将该网络上拉。因此该版本的接收功能可能不可用。请勿将此限制泛化至未审查的硬件版本。

引脚请参阅 `docs/board_ZH.md`，无线协议约定请参阅 `docs/p4-c6-hosted-wifi_ZH.md`。共享 Build
工作流的编译成功不代表硬件验证。
