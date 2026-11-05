# IP101 以太网

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用 IP101 兼容 PHY 启动 ESP32-P4 EMAC，并通过 DHCP 获取 IPv4 地址。

## 硬件需求

需要 ESP32-P4-WIFI6-Touch-LCD-4B 开发板、兼容的 ESP32-P4-86-Panel-ETH-2RO 组件和以太网连接。

## 怎么运行

使用 ESP-IDF v5.5.6 或 v6.1；Board Manager 会解析 `waveshare/waveshare_boards` >=0.3.1 和 `espressif/esp_board_manager` >=0.7.5 依赖。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

监视器会报告链路状态和 DHCP 分配的 IPv4 地址。

## 常见问题

重新尝试 DHCP 前，请检查兼容底板连接和以太网网络。
