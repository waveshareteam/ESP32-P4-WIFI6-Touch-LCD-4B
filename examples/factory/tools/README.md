# Factory C6 Update

[English](README.md) | [简体中文](README_ZH.md)

The factory SPIFFS image carries `c6/esp_hosted-3.0.10.bin`. With factory firmware running, install the paired C6 image using `python tools/flash_c6.py --port <PORT> --bundled`; the tool reports success after reboot and an exact C6 version query. To install another validated C6 application for diagnostics, use `python tools/flash_c6.py --port <PORT> <C6_APP.bin>`.
