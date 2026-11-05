# IP101 Ethernet

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Starts the ESP32-P4 EMAC with the IP101-compatible PHY and obtains an IPv4 address through DHCP.

## Hardware Required

Use an ESP32-P4-WIFI6-Touch-LCD-4B board with the compatible ESP32-P4-86-Panel-ETH-2RO assembly and an Ethernet connection.

## How to Run

Use ESP-IDF v5.5.3, v5.5.6, or v6.1; Board Manager resolves `waveshare/waveshare_boards` >=0.3 and `espressif/esp_board_manager` >=0.7.5 as dependencies.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## Expected Result

The monitor reports link status and the DHCP-assigned IPv4 address.

## Troubleshooting

Check the compatible bottom-board connection and Ethernet network before retrying DHCP.
