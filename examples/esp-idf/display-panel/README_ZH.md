# 显示与触摸面板

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用 LVGL 初始化 4 英寸 720 x 720 显示屏和 GT911 触摸面板。示例依次显示红、绿、蓝、白屏幕，然后在触摸位置绘制方块。

## 硬件需求

需要一块 ESP32-P4-WIFI6-Touch-LCD-4B 开发板。

## 怎么运行

使用 ESP-IDF v5.5.3、v5.5.6 或 v6.1；Board Manager 会解析 `waveshare/waveshare_boards` >=0.3 和 `espressif/esp_board_manager` >=0.7.5 依赖。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

面板依次显示色彩屏幕，并在触摸处显示黑色方块。

## 常见问题

暂无。
