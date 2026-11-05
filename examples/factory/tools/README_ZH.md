# 出厂 C6 升级工具

[English](README.md) | [简体中文](README_ZH.md)

出厂 SPIFFS 镜像包含 `c6/esp_hosted-3.0.10.bin`。运行出厂固件后，用 `python tools/flash_c6.py --port <PORT> --bundled` 安装配套 C6 镜像；工具会在重启并查询到准确版本后报告成功。诊断时如需安装另一份经过验证的 C6 应用镜像，可运行 `python tools/flash_c6.py --port <PORT> <C6_APP.bin>`。
