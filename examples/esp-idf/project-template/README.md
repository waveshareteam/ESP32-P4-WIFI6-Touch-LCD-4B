# Project template

[中文](README_ZH.md)

Minimal ESP-IDF application layout for the ESP32-P4-WIFI6-Touch-LCD-4B. Use it as a clean starting point before adding display, touch, audio, storage, networking, or bottom-board features.

## Use

```sh
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

The repository CI targets ESP-IDF v5.5.5 and v6.0.2. No peripheral is initialized by this template, so it is also the quickest way to check a new toolchain setup. Runtime validation still requires the physical board.
