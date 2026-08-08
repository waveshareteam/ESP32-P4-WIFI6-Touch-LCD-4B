# MicroSD card

Mounts a FAT filesystem through the ESP32-P4 SDMMC host, writes and reads test files, then unmounts the card.

| Signal | GPIO |
| --- | ---: |
| D0-D3 | 39-42 |
| CLK | 43 |
| CMD | 44 |
| SD power / VO4 control | 45 / on-chip LDO channel 4 |

```sh
idf.py set-target esp32p4
idf.py menuconfig
idf.py flash monitor
```

Formatting on mount failure is disabled unless explicitly selected. Back up the card before enabling any formatting option. The repository CI targets ESP-IDF v5.5.5 and v6.0.2; card insertion, power, mount, and read/write behavior require board testing.
