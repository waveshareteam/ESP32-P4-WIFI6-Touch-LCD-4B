# MicroSD 卡

[English](README.md)

通过 ESP32-P4 SDMMC 主机挂载 FAT 文件系统，写入和读取测试文件，然后卸载该卡。

| 信号 | GPIO |
| --- | ---: |
| D0-D3 | 39-42 |
| CLK | 43 |
| CMD | 44 |
| SD power / VO4 control | 45 / 片上 LDO 通道 4 |

```sh
idf.py set-target esp32p4
idf.py menuconfig
idf.py flash monitor
```

除非明确选择，否则在挂载失败时不会格式化。启用任何格式化选项前请备份该卡。仓库 CI 面向 ESP-IDF v5.5.5 和 v6.0.2；插卡、供电、挂载和读写行为需要开发板测试。
