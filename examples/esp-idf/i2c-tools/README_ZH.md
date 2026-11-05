# I2C 工具

[English](README.md) | [简体中文](README_ZH.md)

## 功能

为开发板 GPIO 7 和 GPIO 8 上的共享 I2C0 总线提供交互式控制台。它支持总线扫描和寄存器访问命令。

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

监视器显示 I2C 控制台提示符并接受命令。

## 常见问题

仅在确认目标设备和寄存器时使用写入命令。
