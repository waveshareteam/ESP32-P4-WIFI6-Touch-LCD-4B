# 音频编解码器

[English](README.md)

通过 Waveshare BSP 3.0.0 音频 API 使用 ES8311 扬声器编解码器和 ES7210 麦克风 ADC。

- I2C 控制：SDA GPIO 7，SCL GPIO 8
- I2S：DOUT 9，LRCK 10，DIN 11，BCLK 12，MCLK 13
- 功率放大器使能：GPIO 53

`idf.py menuconfig` 可选择以下两种模式之一：

- 随附的 16 kHz 立体声 PCM 播放（默认）；
- 使用 STD TX 和四时隙 TDM RX 的 24 kHz FL/FR 麦克风回声。

TDM 模式使用 BSP 文档规定的时隙顺序和通道掩码；它不提供声学回声消除。扬声器、麦克风、增益、通道顺序和反馈行为仍需硬件测试。

`main/canon.pcm` 来自官方产品示例归档包，但未提供其原始来源和再分发条款。再分发前请审阅 [`../../../docs/licensing_ZH.md`](../../../docs/licensing_ZH.md)。
