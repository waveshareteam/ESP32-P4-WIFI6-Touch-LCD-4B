# Audio codecs

Exercises the ES8311 speaker codec and ES7210 microphone ADC through the Waveshare BSP 3.0.0 audio API.

- I2C control: SDA GPIO 7, SCL GPIO 8
- I2S: DOUT 9, LRCK 10, DIN 11, BCLK 12, MCLK 13
- power amplifier enable: GPIO 53

`idf.py menuconfig` selects one of two modes:

- bundled 16 kHz stereo PCM playback (default);
- 24 kHz FL/FR microphone echo using STD TX and four-slot TDM RX.

The TDM mode uses the BSP's documented slot order and channel masks; it does not provide acoustic echo cancellation. Speaker, microphone, gain, channel order, and feedback behavior still require hardware testing.

`main/canon.pcm` came from the official product example archive, but its original source and redistribution terms were not supplied. Review [`../../../docs/licensing.md`](../../../docs/licensing.md) before redistribution.
