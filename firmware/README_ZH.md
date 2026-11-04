# 固件

[English](README.md) | [简体中文](README_ZH.md)

合并固件从 `0x0` 烧录。请按 ESP32-P4 芯片版本选择对应镜像。

| 文件 | 用途 |
| --- | --- |
| [ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin) | ESP32-P4 rev1.x 出厂固件，自动选择 ST7703/JD9365 参数 |
| [ESP32-P4-WIFI6-Touch-LCD-4B-Brookesia-rev3_x-260821.bin](ESP32-P4-WIFI6-Touch-LCD-4B-Brookesia-rev3_x-260821.bin) | 之前的 ESP32-P4 rev3.x Brookesia 固件 |

烧录最新出厂固件：

```bash
esptool.py --chip esp32p4 --port <PORT> erase_flash
esptool.py --chip esp32p4 --port <PORT> write_flash 0x0 ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin
```

源码：[examples/factory](../examples/factory/)。
