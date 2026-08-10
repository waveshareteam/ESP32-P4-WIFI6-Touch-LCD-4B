# Contributing

[English](CONTRIBUTING.md) · [简体中文](CONTRIBUTING_ZH.md)

Keep changes focused and preserve the canonical repository layout. Place
first-party ESP-IDF projects under `examples/esp-idf/` and first-party Arduino
sketches under `examples/arduino/`.

Before opening a pull request:

1. Build every changed example with its supported framework versions.
2. Run `idf.py set-target esp32p4` before building ESP-IDF projects.
3. Update documentation when dependencies, board requirements, or compatibility
   contracts change.
4. Do not commit build output, credentials, generated archives, or host-specific
   configuration.

Pull requests should state the exact framework version and board revision used
for validation.
