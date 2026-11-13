# Factory Firmware

[English](README.md) | [简体中文](README_ZH.md)

Flash the combined image at `0x0`. Choose the image that matches the ESP32-P4 silicon revision.

| File | Purpose |
| --- | --- |
| [Factory rev1.x](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_rev1_x_20261113.bin) | Silicon 1.00–1.99; display verified on rev1.3 |
| [Factory rev3.x](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_rev3_x_20261104.bin) | Silicon 3.00–3.99; build and image bounds verified, hardware test pending |

```bash
esptool.py --chip esp32p4 --port <PORT> write_flash 0x0 <IMAGE.bin>
```

Both use the same [factory source](../examples/factory/README.md), automatic JD9365DA/ST7703 selection, and the paired Hosted/C6 3.0.10 stack. For an older C6 image, use the [bundled P4-driven installer](../examples/factory/tools/README.md).
