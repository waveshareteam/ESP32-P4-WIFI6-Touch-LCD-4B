# RS485 回显

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过兼容底板上的 RS485 收发器接收数据，并将其回送到总线。

## 硬件需求

需要 ESP32-P4-WIFI6-Touch-LCD-4B 开发板、兼容的 ESP32-P4-86-Panel-ETH-2RO 底板和 RS485 对端。

## 怎么运行

使用 ESP-IDF v5.5.3、v5.5.6 或 v6.1；Board Manager 会解析 `waveshare/waveshare_boards` >=0.3 和 `espressif/esp_board_manager` >=0.7.5 依赖。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

从 RS485 对端收到的数据会被回送，并在监视器中报告。

## 常见问题

将 A 接 A、B 接 B，并为 RS485 网络使用合适的公共参考地和终端。
