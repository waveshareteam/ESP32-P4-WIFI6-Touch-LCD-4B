# 项目模板

[English](README.md) | [简体中文](README_ZH.md)

## 功能

提供用于开始 ESP32-P4-WIFI6-Touch-LCD-4B 项目的最小 ESP-IDF 应用布局。

## 硬件需求

运行该应用需要一块 ESP32-P4-WIFI6-Touch-LCD-4B 开发板。

## 怎么运行

使用 ESP-IDF v5.5.3、v5.5.6 或 v6.1；Board Manager 会解析 `waveshare/waveshare_boards` >=0.3 和 `espressif/esp_board_manager` >=0.7.5 依赖。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

监视器打印模板应用的启动消息。

## 常见问题

暂无。
