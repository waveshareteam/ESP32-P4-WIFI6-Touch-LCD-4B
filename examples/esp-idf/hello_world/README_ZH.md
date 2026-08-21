# Hello World

[English](README.md)

此最小项目验证仓库、工具链和 ESP32-P4 目标配置正确。它不初始化显示屏、触摸控制器、音频硬件、存储或无线协处理器。

## 构建

在仓库根目录且 ESP-IDF 环境已激活时：

```sh
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

若要烧录并监视，请将串口替换为开发板使用的端口：

```sh
idf.py -C examples/esp-idf/hello_world -p PORT flash monitor
```
