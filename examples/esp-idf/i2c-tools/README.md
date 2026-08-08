# I2C tools

Interactive I2C console for the board's shared bus:

- SDA: GPIO 7
- SCL: GPIO 8
- default port: I2C0

The console provides `i2cconfig`, `i2cdetect`, `i2cget`, `i2cset`, and `i2cdump`. The same bus is used by the GT911 touch controller and the audio codecs, so writes to detected devices can change live hardware state.

```sh
idf.py set-target esp32p4
idf.py menuconfig
idf.py flash monitor
```

Configure the console transport under **Example Configuration**. The repository CI targets ESP-IDF v5.5.5 and v6.0.2; hardware scanning and register access are not validated by compilation alone.
