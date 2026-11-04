# 出厂固件

[English](README.md) | [简体中文](README_ZH.md)

## 功能

构建 ESP32-P4-WIFI6-Touch-LCD-4B 出厂应用，并选择匹配的 ST7703 或 JD9365 显示配置。

## 硬件需求

需要一块 ESP32-P4-WIFI6-Touch-LCD-4B 开发板。

## 怎么运行

使用正式的出厂固件 ESP-IDF v5.5.6 环境；Board Manager 会解析 `waveshare/waveshare_boards` >=0.3 和 `espressif/esp_board_manager` >=0.7.5 依赖。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

产品 UI 使用匹配的显示配置启动。

## 常见问题

仓库也使用 v5.5.3 和 v6.1 构建 ESP-IDF 示例；请使用 [ESP-Hosted 工具](https://github.com/espressif/esp-hosted/blob/master/docs/tools.md)处理随附的 ESP32-C6 镜像。
