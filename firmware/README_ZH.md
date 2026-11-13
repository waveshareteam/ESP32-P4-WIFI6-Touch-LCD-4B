# 出厂固件

[English](README.md) | [简体中文](README_ZH.md)

合并固件从 `0x0` 烧录，请按 ESP32-P4 芯片版本选择镜像。

| 文件 | 用途 |
| --- | --- |
| [出厂 rev1.x](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_rev1_x_20261113.bin) | 芯片版本 1.00–1.99；已在 rev1.3 设备验证显示正常 |
| [出厂 rev3.x](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_rev3_x_20261104.bin) | 芯片版本 3.00–3.99；已通过构建和镜像边界校验，待实机测试 |

```bash
esptool.py --chip esp32p4 --port <PORT> write_flash 0x0 <IMAGE.bin>
```

两份固件使用同一份[出厂源码](../examples/factory/README_ZH.md)，自动识别 JD9365DA/ST7703，并配套 Hosted/C6 3.0.10。旧版 C6 可使用[由 P4 执行的升级工具](../examples/factory/tools/README_ZH.md)。
