# MicroSD 卡

[English](README.md) | [简体中文](README_ZH.md)

## 功能

在 microSD 卡上挂载 FAT 文件系统，写入并读取测试文件，然后卸载该卡。

## 硬件需求

需要一块 ESP32-P4-WIFI6-Touch-LCD-4B 开发板和一张 microSD 卡。

## 怎么运行

使用 ESP-IDF v5.5.3、v5.5.6 或 v6.1；Board Manager 会解析 `waveshare/waveshare_boards` >=0.3 和 `espressif/esp_board_manager` >=0.7.5 依赖。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

监视器报告卡已挂载，且测试文件已写入和读取。

## 常见问题

在 `idf.py menuconfig` 中启用任何格式化选项前请备份存储卡。
