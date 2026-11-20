# 固件

[English](README.md) | [简体中文](README_ZH.md)

ESP32-P4-WIFI6-Touch-LCD-4B 的出厂固件。每个 BIN 都是从 `0x0` 烧录的合并镜像，文件名中的日期即版本。请按 ESP32-P4 芯片版本选择镜像，注意与 PCB 版本区分。

这两份固件同样适用于 `ESP32-P4-86-Panel-ETH-2RO`，请根据其 ESP32-P4 芯片版本选择对应镜像。

| 文件 | 用途 |
| --- | --- |
| [ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261113.bin](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261113.bin) | 出厂固件，适用于 ESP32-P4 芯片版本 1.00–1.99 |
| [ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin) | 出厂固件，适用于 ESP32-P4 芯片版本 3.00–3.99 |

在本目录运行以下命令，将 `<PORT>` 替换为开发板对应的串口。整片擦除会清除已有设置和用户数据。

```bash
python -m pip install esptool
esptool.py --chip esp32p4 --port <PORT> erase_flash
```

随后只运行与芯片版本匹配的一条烧录命令。

rev1.x：

```bash
esptool.py --chip esp32p4 --port <PORT> write_flash 0x0 ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261113.bin
```

rev3.x：

```bash
esptool.py --chip esp32p4 --port <PORT> write_flash 0x0 ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin
```

详见[出厂源码与显示屏适配说明](../examples/factory/README_ZH.md)及 [C6 固件升级说明](../examples/factory/tools/README_ZH.md)。
