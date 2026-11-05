# Audio Codec

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Exercises the ES8311 speaker codec and ES7210 microphone ADC through the board audio API. Select PCM playback or microphone echo in `idf.py menuconfig`.

## Hardware Required

An ESP32-P4-WIFI6-Touch-LCD-4B development board is required.

## How to Run

Use ESP-IDF v5.5.3, v5.5.6, or v6.1; Board Manager resolves `waveshare/waveshare_boards` >=0.3 and `espressif/esp_board_manager` >=0.7.5 as dependencies.

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b
idf.py set-target esp32p4
idf.py build flash monitor
```

## Expected Result

The selected playback or microphone-echo mode starts and reports its status in the monitor.

## Troubleshooting

Review the [`canon.pcm` licensing note](../../../docs/licensing.md) before redistributing the bundled audio file.
