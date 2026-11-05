# 音频编解码器

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过开发板音频 API 使用 ES8311 扬声器编解码器和 ES7210 麦克风 ADC。可在 `idf.py menuconfig` 中选择 PCM 播放或麦克风回声。

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

所选的播放或麦克风回声模式启动，并在监视器中报告状态。

## 常见问题

重新分发随附音频文件前，请阅读 [`canon.pcm` 许可说明](../../../docs/licensing_ZH.md)。
