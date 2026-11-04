# 已归档的固件组件

[English](README.md) · [简体中文](README_ZH.md)

此目录保存有意排除在 ESP-IDF 组件发现路径之外的实现快照。

components/AIChats 包含原先独立的 Xiaozhi 协议实现。在活跃的
components/XiaozhiApp 应用使用托管的 espressif/esp_xiaozhi 组件期间，它仅作为迁移参考保留。

不要将此目录添加到 EXTRA_COMPONENT_DIRS。生产性更改应位于活跃组件树中。
