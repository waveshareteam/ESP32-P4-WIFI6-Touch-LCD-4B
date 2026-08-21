# 开发板信息

[English](board.md) · [简体中文](board_ZH.md)

本文档区分独立的 ESP32-P4-WIFI6-Touch-LCD-4B 与相关的 ESP32-P4-86-Panel-ETH-2RO
装配体及其底板。官方产品页面表明二者的主 PCB 与外壳配置不同；文档未将该底板
描述为独立 4B 的即插即用选项。
引脚事实来自官方产品文档、`schematic/README.md` 中列出的原理图以及当前的
板级支持配置。某个引脚已被文档记录，本身并不能证明相应外设已经在实体硬件上
测试过。

## 主显示板

| 项目 | 值 |
| --- | --- |
| 产品 | ESP32-P4-WIFI6-Touch-LCD-4B |
| ESP-IDF 目标 | `esp32p4` |
| 默认示例基线 | ESP32-P4 rev3.x / `postv3` 设置（`SELECTS_REV_LESS_V3=n`、3.00-3.99、250 MHz PSRAM） |
| 维护固件 profile | 独立的 `rev1_3` 与 `rev3_x` Brookesia 芯片/配置 profile |
| Flash | 32 MB |
| PSRAM | 32 MB；维护的 Brookesia 默认配置中为 Hex 模式 |
| 显示屏 | 4 英寸、720 x 720、双通道 MIPI DSI、ST7703 |
| 触摸 | GT911 电容式触摸 |
| 播放编解码器 | ES8311 |
| 录音编解码器 | ES7210 |
| 无线协处理器 | 通过 SDIO 连接的 ESP32-C6-MINI-1U-H8 |
| 摄像头 | MIPI CSI 连接器；当前固件默认使用 OV5647 模式 |
| 存储 | 四线 SDMMC MicroSD 卡槽 |

### 显示与触摸

| 功能 | 信号 | GPIO / 设置 |
| --- | --- | --- |
| LCD | 分辨率 | 720 x 720 |
| LCD | 接口 | 双通道 MIPI DSI |
| LCD | Registry BSP 3.0.1 中的 DSI 通道速率 | 每通道 480 Mbit/s |
| LCD | 复位 | GPIO27 |
| LCD 背光 | PWM | GPIO26 |
| LCD 背光 | 使能 | GPIO33 |
| 触摸 / 共享控制总线 | SDA | GPIO7 |
| 触摸 / 共享控制总线 | SCL | GPIO8 |
| 触摸 | 复位 | 原理图有 GPIO23；Arduino 辅助库与已发布 BSP 3.0.1 均不驱动该引脚 |
| 触摸 | 中断 | 仅测试点；未连接至 MCU GPIO，Arduino 辅助库与已发布 BSP 3.0.1 均不使用 |

本产品的 LCD 复位与触摸复位为独立信号。Arduino 辅助库有意不驱动 GT911 的 INT 或 RST：
它先探测 I2C 地址 `0x5D`，再探测 `0x14`，以响应的地址初始化，并轮询触摸状态。已发布的 BSP
3.0.1 使用同样行为。不要从另一块 Waveshare 开发板复制“共用复位”的假设。编译不能验证实体
硬件上的地址选择时序或触摸输入。

### 音频

| 信号 | GPIO |
| --- | ---: |
| 共享编解码器 I2C SDA / SCL | 7 / 8 |
| I2S 播放数据输出 | 9 |
| I2S LRCK / 字选择 | 10 |
| I2S 录音数据输入 | 11 |
| I2S 位时钟 | 12 |
| I2S 主时钟 | 13 |
| 功率放大器使能 | 53 |

维护的固件使用 ES8311 播放、ES7210 录音。其在开发板上观测到的 ES7210 串行化 TDM 顺序为
MIC1、MIC3（回声参考）、MIC2、MIC4。物理麦克风选择掩码与串行化 TDM 时隙掩码属于不同的
命名空间，不能互换。

### MicroSD

| 信号 | GPIO |
| --- | ---: |
| D0 | 39 |
| D1 | 40 |
| D2 | 41 |
| D3 | 42 |
| CLK | 43 |
| CMD | 44 |
| 电源控制 | 45 |

### ESP32-C6 无线协处理器

P4 主机将 GPIO6、GPIO14、GPIO16、GPIO18、GPIO19、GPIO20、GPIO22 和 GPIO54 用于
C6 的 SDIO/控制连接。信号级映射与时序由 Hosted 配置和匹配的 C6 固件决定；启用无线支持时，
不要将这些引脚复用为通用 GPIO。

官方示例压缩包不包含 C6 从机源代码、二进制文件或构建说明。请参阅
`docs/p4-c6-hosted-wifi_ZH.md`。

### 摄像头与共享总线

摄像头使用专用 MIPI CSI 接口，并共享 GPIO7/GPIO8 I2C 控制总线。在启用并非随该开发板提供的
摄像头前，请验证传感器电压、排线方向和所选传感器模式。

## 86 面板底板

以太网、继电器和 RS485 位于相关 86 面板产品装配体的底板上。它们不是独立 4B 开发板的功能，
本仓库也不声称两种主 PCB 或外壳配置可以互换。

| 功能 | 信号 | GPIO / 设置 |
| --- | --- | --- |
| 继电器 1 | 高电平有效输出 | GPIO32 |
| 继电器 2 | 高电平有效输出 | GPIO46 |
| RS485 | UART1 TX | GPIO47 |
| RS485 | UART1 RX | GPIO48 |
| RS485 | 示例串口格式 | 115200、8 数据位、无校验、1 停止位 |
| IP101 | PHY 地址 | 1 |
| IP101 | 复位 | GPIO51 |
| IP101 | MDC / MDIO | GPIO31 / GPIO52 |
| IP101 | RMII 时钟输入 | GPIO50 |
| IP101 | TX_EN / TXD0 / TXD1 | GPIO49 / GPIO34 / GPIO35 |
| IP101 | CRS_DV / RXD0 / RXD1 | GPIO28 / GPIO29 / GPIO30 |

当前官方产品文档将 RS485 接口描述为使用自动方向控制；因此应用仅使用 UART TX/RX，
不使用单独的方向 GPIO。在依赖接收操作前，请确认实际底板版本。

项目专用的继电器、以太网和 RS485 组合逻辑属于 `bsp_extra`；在没有匹配的共享开发板设计时，
不应将其移入可复用的显示/触摸 BSP。

### 历史 V1.3 RS485 差异

在已审阅的历史文件
`schematic/86 Panel Bottom Board V1.3(1).pdf` 中，RS485 收发器的 `/RE` 与 `DE` 信号
被连接在一起，该网络通过 4.7 kOhm 上拉。该状态选择发送模式并禁用接收器。因此，该 V1.3
开发板可能无法进行接收操作。

这是针对特定版本原理图的结论。在将其应用于其他版本前，请确认实际底板和当前官方原理图。

不要在回环测试中将 RS485 A 与 B 短接。这会使差分总线失效。在方向控制和总线所有权明确前，
不要连接另一个主动驱动的 RS485 适配器。

## BSP 基线

仓库迁移基线是已发布的 `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.1 版本：

- ESP-IDF `>=5.5`。
- `espressif/esp_lvgl_adapter ~0.6`。
- `waveshare/esp_lcd_st7703 ^2.0.0`。
- `espressif/esp_lcd_touch_gt911 ^1`。
- `espressif/esp_codec_dev ~1.5`。
- 使用 ESP-IDF 6 或更高版本构建时使用 `espressif/usb ^1.0.0`。

需要板级或显示支持的首方 ESP-IDF 示例，以及受维护的 Brookesia 固件，均从
ESP Component Registry 解析可复用的 BSP 与 ST7703 驱动。本地不再保留会遮蔽
解析的同名副本，只保留产品专用的 `bsp_extra` 代码。

产品 manifest 通过 Component Registry 解析已发布的 BSP 3.0.1。不能以 Git URL 替代
Registry 发布版本。

## 验证状态

`rev1_3` 与 `rev3_x` 是芯片/配置 profile，不是已经验证的 PCB 电气版本。`rev1_3` 为
覆盖芯片版本 1.00 至 1.99、使用 200 MHz PSRAM 基线；`rev3_x` 覆盖芯片版本 3.00 至 3.99
（最高排他版本 4.00）、使用 250 MHz PSRAM 基线。二者的二进制不能交叉烧录。默认 ESP-IDF 示例和 Arduino 草图
使用 `rev3_x`/post-v3，不会扩展为双矩阵；只有 Brookesia 继续构建两个 profile。v3.x
profile 需要 ESP-IDF 5.5.3 或更高版本（或 6.0 及更高版本），本仓库的受维护固件工作流
当前固定为 v5.5.5。现有主板原理图不足以证明这两个 profile 名称之间存在 PCB/电气差异；
芯片探测或编译同样不能证明硬件兼容性。

### 由源材料支持

- 上述主板与底板引脚表。
- 显示控制器、分辨率、触摸控制器、编解码器、C6 模块、Flash 与 PSRAM 容量。
- 历史 V1.3 RS485 方向网络拓扑。

### 编译验证

成功的 ESP-IDF 或 Arduino 构建会检查所选框架版本的源代码/API 兼容性。应单独记录工作流结果及其
确切依赖解析；本文档不会仅因存在矩阵条目便声称构建已经通过。

### 仍需硬件验证

- 确认主板、底板和 ESP32-P4 芯片版本。
- 显示屏、触摸、背光、扬声器、所有已连接麦克风和音频时钟。
- 摄像头传感器及受支持的模式。
- MicroSD、USB、C6 Wi-Fi、IP101 以太网、继电器和 RS485 行为。
- 确切的 C6 从机固件和兼容的 P4 主机栈。

记录结果时，请包含硬件版本、框架版本、示例路径、依赖版本，以及测试是仅编译还是在实体开发板上进行。
