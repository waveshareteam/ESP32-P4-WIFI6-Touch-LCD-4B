# Wi-Fi station through ESP32-C6

[中文](README_ZH.md)

Runs the standard station example on ESP32-P4 while `esp_wifi_remote` uses `esp_hosted` and the board's ESP32-C6 coprocessor over SDIO.

Pinned host components:

- `espressif/esp_wifi_remote` 1.6.3
- `espressif/esp_hosted` 2.12.11

Set the SSID and password with `idf.py menuconfig` under **Example Configuration**. The application deliberately does not print the password.

```sh
idf.py set-target esp32p4
idf.py menuconfig
idf.py flash monitor
```

The official product example archive does not contain the ESP32-C6 slave source, image, or a recovery procedure. A successful P4 build therefore does not prove that the C6 currently on the board implements the matching Hosted protocol. See [`../../../docs/p4-c6-hosted-wifi.md`](../../../docs/p4-c6-hosted-wifi.md).
