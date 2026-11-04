# Firmware

[English](README.md) | [简体中文](README_ZH.md)

Combined firmware images for flashing at `0x0`. Keep the image matched to the ESP32-P4 silicon revision.

| File | Purpose |
| --- | --- |
| [ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin) | Factory firmware for ESP32-P4 rev1.x, with automatic ST7703/JD9365 selection |
| [ESP32-P4-WIFI6-Touch-LCD-4B-Brookesia-rev3_x-260821.bin](ESP32-P4-WIFI6-Touch-LCD-4B-Brookesia-rev3_x-260821.bin) | Previous Brookesia firmware for ESP32-P4 rev3.x |

Flash the latest factory image:

```bash
esptool.py --chip esp32p4 --port <PORT> erase_flash
esptool.py --chip esp32p4 --port <PORT> write_flash 0x0 ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin
```

Source: [examples/factory](../examples/factory/).
