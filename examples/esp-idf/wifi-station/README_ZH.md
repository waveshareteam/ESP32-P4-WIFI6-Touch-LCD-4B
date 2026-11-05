# 通过 ESP32-C6 的 Wi-Fi 站点模式

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过开发板的 ESP32-C6 协处理器在 ESP32-P4 上运行 ESP-IDF 站点示例。构建前请在 `idf.py menuconfig` 中设置网络凭据。

## 硬件需求

需要一块 ESP32-P4-WIFI6-Touch-LCD-4B 开发板和可用的 Wi-Fi 网络。

## 怎么运行

使用 ESP-IDF v5.5.3、v5.5.6 或 v6.1；Board Manager 会解析 `waveshare/waveshare_boards` >=0.3 和 `espressif/esp_board_manager` >=0.7.5 依赖。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

连接后，监视器报告站点网络事件和分配的地址。

## 常见问题

重新连接前，请确认网络凭据以及 ESP32-C6 上匹配的 ESP-Hosted 固件。
