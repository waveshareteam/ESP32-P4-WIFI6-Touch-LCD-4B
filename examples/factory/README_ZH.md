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
idf.py -B build/rev1_x -DFACTORY_SILICON_PROFILE=rev1_x set-target esp32p4
idf.py -B build/rev1_x -DFACTORY_SILICON_PROFILE=rev1_x build
```

`rev1_x` 是默认配置，适用于 ESP32-P4 1.00-1.99 版本。对于 3.00-3.99
版本，请使用独立的构建目录和 SDKCONFIG 路径：

```bash
idf.py -B build/rev3_x -DFACTORY_SILICON_PROFILE=rev3_x set-target esp32p4
idf.py -B build/rev3_x -DFACTORY_SILICON_PROFILE=rev3_x build
```

请勿在同一构建目录中切换 `FACTORY_SILICON_PROFILE`。如传入
`-DSDKCONFIG=...`，两个配置必须使用不同的 SDKCONFIG 文件。

## 预期现象

产品 UI 使用匹配的显示配置启动。应用镜像版本会显示为所选配置对应的
`factory-rev1x` 或 `factory-rev3x`。

## 常见问题

仓库也使用 v5.5.6 和 v6.1 构建 ESP-IDF 示例；请使用 [ESP-Hosted 工具](https://github.com/espressif/esp-hosted/blob/master/docs/tools.md)处理随附的 ESP32-C6 镜像。
