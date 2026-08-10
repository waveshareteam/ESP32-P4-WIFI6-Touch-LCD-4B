# IP101 Ethernet

[中文](README_ZH.md)

Starts the ESP32-P4 internal EMAC with the board's IP101-compatible PHY and obtains an IPv4 address through DHCP.

| Signal | GPIO |
| --- | ---: |
| MDC / MDIO / PHY reset | 31 / 52 / 51 |
| RMII reference clock | 50 |
| TX enable / TXD0 / TXD1 | 49 / 34 / 35 |
| CRS_DV / RXD0 / RXD1 | 28 / 29 / 30 |

The physical RJ45 connection belongs to the related
ESP32-P4-86-Panel-ETH-2RO assembly; the standalone 4B board does not expose an
RJ45 socket, and its main PCB must not be assumed interchangeable. The example
uses the cross-version generic IEEE 802.3 PHY driver so the same source can
target ESP-IDF v5.5.5 and v6.0.2.

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

Link, magnetics, DHCP, throughput, and long-duration stability require hardware and network testing.
