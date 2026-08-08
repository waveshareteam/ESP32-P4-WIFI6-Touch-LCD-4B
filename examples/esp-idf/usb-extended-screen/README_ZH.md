# USB 扩展屏幕

[English](README.md)

此 ESP-IDF 示例将 Waveshare ESP32-P4-WIFI6-Touch-LCD-4B 变为固定尺寸的 720 x 720 USB 辅助显示器。它基于 Waveshare 产品示例和乐鑫 `usb_extend_screen` 实现，并针对已发布的 Waveshare BSP 3.0.0 规范化了开发板集成。

这是一种厂商专用 USB 显示设备，不是 USB 视频类（UVC）设备，也不是基于标准的 DisplayPort/USB 显示器。兼容的 Windows 间接显示驱动程序（IDD）通过 TinyUSB 厂商端点发送压缩帧。

## 数据路径

```text
Windows 桌面
  -> 兼容 Espressif 的 IDD
  -> USB 2.0 高速厂商 OUT 端点
  -> 有界 JPEG 帧队列
  -> ESP32-P4 硬件 JPEG 解码器
  -> 720 x 720 RGB565 MIPI-DSI 帧缓冲区
```

默认复合设备还提供：

- 最多五个 GT911 触点的 HID 数字化仪报告；以及
- USB 音频类扬声器和麦克风接口。

可在 `menuconfig` 中独立禁用 HID 和 UAC。如果两者均被禁用，示例将使用下文所述的仅厂商产品 ID。

## 硬件和软件范围

- 开发板：Waveshare ESP32-P4-WIFI6-Touch-LCD-4B。
- 面板：4 英寸、兼容 ST7703 的 720 x 720 MIPI-DSI 面板。
- 触摸：GT911，HID 描述符表示五个触点。
- USB：ESP32-P4 原生 USB 2.0 device/OTG 连接，配置为高速 UTMI PHY 路径。
- ESP-IDF 源码范围：`>=5.5,<7.0`。
- 开发板组件：`waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.0。
- LVGL：9.3.0，搭配 `espressif/esp_lvgl_adapter` 0.6.3。
- UAC 组件：`espressif/usb_device_uac` 1.3.1。
- TinyUSB 组件：`espressif/tinyusb` `0.19.0~3`（精确版本）。
- 保留的可选播放器辅助组件：`chmorgan/esp-audio-player` 1.0.7，搭配 `chmorgan/esp-file-iterator` 1.0.0。

请在原生 USB 端口使用支持数据传输的线缆。仅串口/JTAG 的连接器无法承载厂商显示流。

## Windows IDD 要求

没有匹配的 IDD 时，Windows 不会将厂商接口识别为显示器。乐鑫在此处提供其主机端驱动程序文档：

- [USB 扩展屏幕 Windows 驱动程序说明](https://github.com/espressif/esp-iot-solution/blob/master/examples/usb/device/usb_extend_screen/windows_driver/README_cn.md)
- [Espressif USB 扩展屏幕示例](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_extend_screen)

上游说明当前链接到以下已签名安装程序：

- [xfz1986_usb_graphic_250224_rc_sign.exe](https://dl.espressif.com/AE/esp-iot-solution/xfz1986_usb_graphic_250224_rc_sign.exe)

该安装程序是外部二进制文件。本仓库未对其进行内置、镜像、校验或再分发。运行前请核验其发布者签名、来源及其是否适用于目标系统。上游驱动程序文档面向 Windows 10/11；本仓库不提供 Linux 或 macOS 显示驱动程序。

### USB 标识

| 配置 | VID | PID |
| --- | ---: | ---: |
| 厂商 + HID 和/或 UAC（默认） | `0x303A` | `0x2986` |
| 仅厂商接口 | `0x303A` | `0x2987` |

IDD 配置必须与固件 VID/PID 匹配。因此，变更 HID 或 UAC 可能改变默认 PID。

## 厂商帧协议

主机写入一个打包的 16 字节小端序标头，后接 `payload_total` 个有效载荷字节。固件以流方式处理有效载荷；一次 USB 传输不必包含完整帧。

| 偏移 | 字段 | 大小 | 当前设备行为 |
| ---: | --- | ---: | --- |
| 0 | `crc16` | 2 字节 | 为兼容上游协议而保留；此固件不验证 |
| 2 | `type` | 1 字节 | 接受 `3`（JPEG）；拒绝 RGB565、RGB888 和 YUV420 |
| 3 | `cmd` | 1 字节 | 由导入的协议保留；不解释 |
| 4 | `x` | 2 字节 | 必须为 `0` |
| 6 | `y` | 2 字节 | 必须为 `0` |
| 8 | `width` | 2 字节 | 必须为 `720` |
| 10 | `height` | 2 字节 | 必须为 `720` |
| 12 | `frame_id` | 10 位 | 位于打包的最后一个字中；不用于排序 |
| 12 | `payload_total` | 22 位 | 声明的 JPEG 长度；必须为 1 至 300000 字节 |

`UDISP_TYPE_END` 值为 `0xFF`。当前渲染路径仅接受全屏 JPEG 帧。未实现局部矩形和原始 RGB/YUV 有效载荷。

接收路径会拒绝过短的标头、意外的几何参数、零长度或超大有效载荷、超出声明有效载荷的数据，以及超出已分配帧缓冲区的写入。当没有空帧可用，或帧类型不受支持时，仍会消费并丢弃剩余的已声明有效载荷，避免将其重新解释为新标头。

此帧格式仍不是经认证的传输。请将连接的 USB 主机及其驱动程序视为受信任方。当前实现中的标头 CRC 字段不是安全或完整性边界。

## 构建和烧录

激活受支持的 ESP-IDF 环境，然后在此目录中运行：

```sh
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

已检入的默认配置选择了 4 英寸 720 x 720 面板、RGB565、三个 DPI 帧缓冲区、32 MB 闪存、PSRAM、USB 高速，以及 48 kHz/16 位立体声 UAC 接口。如果连接的硬件或所需的 USB 组合不同，请在构建前使用 `idf.py menuconfig`。

## 验证边界

源码已规范化并经过静态审查。作为此次更新的一部分，未执行本地构建、烧录、USB 枚举、Windows IDD 测试、显示测试、音频测试或触摸测试。请查阅仓库的 [CI 状态](../../../docs/ci_ZH.md) 获取编译结果，不要将此 README 视为构建证书。

上述可选播放器辅助组件版本是从导入的 `bsp_extra` 组件进行的静态依赖选择。在此次更新期间未解析或编译验证，且不用于 USB 显示数据路径。

硬件冒烟测试至少必须覆盖：

1. 使用所选复合 VID/PID 进行高速枚举。
2. 重复发送格式错误和超大厂商帧时不发生队列损坏。
3. 持续进行 720 x 720 JPEG 更新及可见的帧顺序。
4. Windows 中五触点的按下、移动、释放和触点 ID 行为。
5. UAC 播放、静音、音量和端点稳定性。
6. 从 ES7210 路径进行 UAC 麦克风采集。

UAC 麦克风路径尚未验证。该开发板将 ES8311 播放与 ES7210 采集结合，而导入的胶水代码会在打开两个编解码器前初始化 BSP 的默认全双工音频路径。声明麦克风支持前，请在硬件上确认所需的 ES7210 TDM 时隙配置。无需音频时，禁用 UAC 是最安全的配置。

## 来源

精确的 Waveshare 压缩包、获取日期、哈希和导入映射记录在 [仓库的源记录](../../../docs/sources.md)中。有用的上游参考资料包括：

- [Waveshare 产品文档](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B)
- [Waveshare BSP 3.0.0](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_4b/versions/3.0.0)
- [Espressif USB Device UAC 1.3.1](https://components.espressif.com/components/espressif/usb_device_uac/versions/1.3.1)

导入来源不代表已进行运行时验证，也不代表获得再分发第三方二进制文件的许可。
