# I2C 工具

[English](README.md)

用于开发板共享总线的交互式 I2C 控制台：

- SDA：GPIO 7
- SCL：GPIO 8
- 默认端口：I2C0

控制台提供 `i2cconfig`、`i2cdetect`、`i2cget`、`i2cset` 和 `i2cdump`。GT911 触摸控制器和音频编解码器共用同一总线，因此写入检测到的设备可能改变实时硬件状态。

```sh
idf.py set-target esp32p4
idf.py menuconfig
idf.py flash monitor
```

在 **Example Configuration** 下配置控制台传输。仓库 CI 面向 ESP-IDF v5.5.5 和 v6.0.2；硬件扫描和寄存器访问无法仅靠编译验证。
