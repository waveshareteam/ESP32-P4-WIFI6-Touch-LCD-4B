# LCD 色彩测试

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 Board Manager 自动选择 JD9365DA/ST7703 屏幕配置，运行 720 x 720 MIPI-DSI 色条测试。

## 硬件需求

需要一块 ESP32-P4-WIFI6-Touch-LCD-4B 开发板。

## 怎么运行

支持的 ESP-IDF 版本以[产品配置](../../../.github/product.json)为准，Board Manager 使用共享板定义。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

面板显示由 MIPI-DSI 主机生成的垂直彩条图案。

## 常见问题

检查启动日志中的屏幕识别结果及 MIPI-DSI 排线连接。
