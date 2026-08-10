# IP101 以太网

[English](README.md)

使用开发板的 IP101 兼容 PHY 启动 ESP32-P4 内部 EMAC，并通过 DHCP 获取 IPv4 地址。

| 信号 | GPIO |
| --- | ---: |
| MDC / MDIO / PHY reset | 31 / 52 / 51 |
| RMII reference clock | 50 |
| TX enable / TXD0 / TXD1 | 49 / 34 / 35 |
| CRS_DV / RXD0 / RXD1 | 28 / 29 / 30 |

物理 RJ45 连接属于相关 ESP32-P4-86-Panel-ETH-2RO 组件；独立 4B 开发板不提供 RJ45 插座，且不应假定其主 PCB 可互换。该示例使用跨版本的通用 IEEE 802.3 PHY 驱动，以便同一份源码可面向 ESP-IDF v5.5.5 和 v6.0.2。

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

链路、磁性元件、DHCP、吞吐量和长期稳定性需要硬件和网络测试。
