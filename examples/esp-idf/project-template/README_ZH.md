# 项目模板

[English](README.md)

ESP32-P4-WIFI6-Touch-LCD-4B 的最小 ESP-IDF 应用布局。在添加显示、触摸、音频、存储、网络或底板功能之前，可将其用作干净的起点。

## 使用

```sh
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

仓库 CI 面向 ESP-IDF v5.5.5 和 v6.0.2。该模板不初始化任何外设，因此也是检查新工具链设置的最快方式。运行时验证仍需要实体开发板。
