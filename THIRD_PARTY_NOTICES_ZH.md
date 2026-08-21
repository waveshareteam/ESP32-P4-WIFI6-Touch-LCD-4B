# 第三方声明

[English](THIRD_PARTY_NOTICES.md) · [简体中文](THIRD_PARTY_NOTICES_ZH.md)

此文件用于协助清点。它不能替代组件附带的许可文本、解决相互冲突的元数据、授予整个仓库的
许可证，也不构成法律意见。分发内容可能有所不同；每次发布时都应审阅并更新此文件。

## Waveshare 官方产品资源

规范化的示例、产品专用的 Arduino 显示/触摸辅助程序、原理图和机械图纸均源自
`docs/sources_ZH.md` 列出的官方 ESP32-P4-WIFI6-Touch-LCD-4B 资源。

在 2026-07-30 审阅的官方示例 ZIP 中，未发现覆盖整个归档的顶层许可证。在镜像或发布缺少
单独条款的材料前，请向权利持有人确认再分发条款。

## Waveshare 托管组件

| 组件 | 版本/基线 | 声明 |
| --- | --- | --- |
| `waveshare/esp32_p4_wifi6_touch_lcd_4b` | 3.0.1 | 上游软件包包含 Apache-2.0 许可文本；该许可仅适用于此组件 |
| `waveshare/esp_lcd_st7703` | 2.0.0 | 许可元数据不一致：清单写为 MIT，附带的 MIT 文本含有未填写的占位符，而源文件头写为 Apache-2.0；需要上游澄清 |

托管依赖项可能不存储在仓库中，但其许可证仍可能适用于包含它们的源代码或二进制分发内容。

## Arduino 库

| 库 | 版本基线 | 许可证声明 |
| --- | --- | --- |
| GFX Library for Arduino | 官方归档中为 1.6.0；仓库 CI 中为 1.6.6 | 上游软件包包含 BSD 风格的许可证文件；请为实际分发的版本保留版权和条件 |
| LVGL | 9.3.0 | 上游软件包包含 MIT 条款；可选库、字体和生成的资源可能带有额外声明 |

仓库可能在构建期间安装这些通用库，而不是将其大型源代码树纳入版本控制。该选择不会消除其许可义务。

## Espressif 派生材料

导入的 ESP-IDF 示例文件和托管的 Espressif 组件可能带有各自的 SPDX 文件头和许可证文件。
请保留属于构建所解析的精确组件版本的这些文件头和许可材料。

本地 Brookesia 核心在 `firmware/brookesia/components/brookesia_core/license.txt` 中包含 Apache-2.0
条款。这些条款适用于该组件，并不为整个仓库选择许可证。

## Xiaozhi 材料

`firmware/brookesia/components/XiaozhiApp/third_party/xiaozhi_esp32/` 下的第三方 Xiaozhi 目录
包含 MIT 条款。请按要求在源代码或二进制分发中保留该许可。如将
`firmware/brookesia/archive/` 下的历史材料纳入发布，必须独立审阅。

## 媒体、字体和美术资源

以下类别需要按文件记录来源并审阅许可：

- `examples/esp-idf/audio-codec/main/canon.pcm`。
- `firmware/brookesia/spiffs/` 下的 OGG 提示音。
- 为 Brookesia 音乐播放器提供的音乐曲目。
- 应用程序图标及其生成的 LVGL C 数组。
- 字体、语音模型和生成的模型镜像。
- 通过托管组件下载的媒体或资源。

不要仅因固件能够使用某项资源就添加或分发它。请记录其来源、作者/权利持有人、精确许可证，
以及任何署名或相同方式共享要求。

## 硬件文档

原理图 PDF 和 DXF/PDF 尺寸图是供应商参考材料。来源审计没有确定开放硬件或通用再分发许可证。
其官方 URL 和哈希可证明出处，而非使用许可。

## 仓库状态

尚未选择仓库范围的 `LICENSE.txt`。在公开发布、再分发或发布固件前，请参阅
`docs/licensing_ZH.md`。
