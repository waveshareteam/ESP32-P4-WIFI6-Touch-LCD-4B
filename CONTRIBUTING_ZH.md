# 参与贡献

[English](CONTRIBUTING.md) · [简体中文](CONTRIBUTING_ZH.md)

请保持更改聚焦，并保留规范的仓库布局。首方 ESP-IDF 项目应放在
`examples/esp-idf/` 下，首方 Arduino 草图应放在 `examples/arduino/` 下。

在发起拉取请求前：

1. 使用每个已更改示例所支持的框架版本构建该示例。
2. 构建 ESP-IDF 项目前运行 `idf.py set-target esp32p4`。
3. 当依赖项、开发板要求或兼容性约定发生变化时更新文档。
4. 不要提交构建输出、凭据、生成的归档文件或与主机相关的配置。

拉取请求应说明用于验证的精确框架版本和开发板版本。
