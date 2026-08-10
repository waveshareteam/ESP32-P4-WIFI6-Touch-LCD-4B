# 通过 ESP32-C6 的 Wi-Fi 站点模式

[English](README.md)

在 ESP32-P4 上运行标准站点示例，同时 `esp_wifi_remote` 通过 SDIO 使用 `esp_hosted` 和开发板的 ESP32-C6 协处理器。

固定的主机组件：

- `espressif/esp_wifi_remote` 1.6.3
- `espressif/esp_hosted` 2.12.11

在 **Example Configuration** 下通过 `idf.py menuconfig` 设置 SSID 和密码。应用特意不会打印密码。

```sh
idf.py set-target esp32p4
idf.py menuconfig
idf.py flash monitor
```

官方产品示例归档包不包含 ESP32-C6 从机源码、镜像或恢复步骤。因此，P4 构建成功并不能证明当前开发板上的 C6 实现了匹配的 Hosted 协议。请参阅 [`../../../docs/p4-c6-hosted-wifi_ZH.md`](../../../docs/p4-c6-hosted-wifi_ZH.md)。
