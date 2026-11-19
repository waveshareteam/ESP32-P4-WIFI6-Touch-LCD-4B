# Firmware

[English](README.md) | [简体中文](README_ZH.md)

Factory firmware for ESP32-P4-WIFI6-Touch-LCD-4B. Each BIN is a merged image flashed at `0x0`; the date in its file name identifies the version. Select the image for your ESP32-P4 silicon revision, not the PCB revision.

| File | Purpose |
| --- | --- |
| [ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261113.bin](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261113.bin) | Factory firmware for ESP32-P4 silicon revisions 1.00–1.99 |
| [ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin](ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin) | Factory firmware for ESP32-P4 silicon revisions 3.00–3.99 |

Run these commands in this directory, replacing `<PORT>` with the board's serial port. Erasing flash removes existing settings and user data.

```bash
python -m pip install esptool
esptool.py --chip esp32p4 --port <PORT> erase_flash
```

Then run only the write command matching your silicon revision.

For rev1.x:

```bash
esptool.py --chip esp32p4 --port <PORT> write_flash 0x0 ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261113.bin
```

For rev3.x:

```bash
esptool.py --chip esp32p4 --port <PORT> write_flash 0x0 ESP32-P4-WIFI6-Touch-LCD-4B_FactoryOnly_20261104.bin
```

See the [factory source and display configuration](../examples/factory/README.md) and [C6 firmware upgrade instructions](../examples/factory/tools/README.md).
