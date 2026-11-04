# USB 扩展屏幕

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用乐鑫厂商 USB 显示协议将开发板变为 720 x 720 USB 辅助显示器。默认复合设备还提供触摸和 USB 音频接口。

## 硬件需求

需要 ESP32-P4-WIFI6-Touch-LCD-4B 开发板、支持数据传输的 USB 线缆，以及装有兼容间接显示驱动程序的 Windows 主机。

## 怎么运行

使用 ESP-IDF v5.5.3、v5.5.6 或 v6.1；Board Manager 会解析 `waveshare/waveshare_boards` >=0.3 和 `espressif/esp_board_manager` >=0.7.5 依赖。

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## 预期现象

安装兼容主机驱动程序后，主机将开发板显示为 720 x 720 辅助显示器。

## 常见问题

请按照[乐鑫 USB 扩展屏幕文档](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_extend_screen)安装和配置兼容驱动程序。
