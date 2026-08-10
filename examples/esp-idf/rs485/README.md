# RS485 echo

[中文](README_ZH.md)

Exercises the RS485 transceiver on the bottom board of the related
ESP32-P4-86-Panel-ETH-2RO assembly. This is not a TTL-UART wiring example; the
standalone 4B board does not expose the RS485 A/B pair, and its main PCB must
not be assumed interchangeable.

## Board configuration

| Function | Default |
| --- | --- |
| UART port | UART1 |
| TX / RX | GPIO 47 / GPIO 48 |
| Serial format | 115200 baud, 8 data bits, no parity, 1 stop bit |
| Bottom-board power | ESP32-P4 on-chip LDO channel 4, 3.3 V |

The current product documentation describes automatic RS485 direction control, so the application uses ordinary UART transmit and receive calls without an RTS direction pin. Received bytes are sent back onto the bus and reported on the debug console.

Configure the UART settings under **Echo Example Configuration**:

```sh
idf.py set-target esp32p4
idf.py menuconfig
idf.py flash monitor
```

## Wiring and safety

Connect A to A, B to B, and provide a common reference ground through an RS485-capable peer. Use termination appropriate to the cable and topology. Never short A and B together for loopback testing, and avoid two devices transmitting at once.

The repository also preserves `schematic/86 Panel Bottom Board V1.3(1).pdf` as a historical reference. In that reviewed V1.3 drawing, `/RE` and `DE` share a pulled-high net, which may leave the receiver disabled. That limitation is revision-specific and must not be generalized to the current official bottom board, whose documentation states automatic direction control.

## Validation boundary

The repository CI targets ESP-IDF v5.5.5 and v6.0.2. Compilation cannot prove the installed transceiver revision, direction timing, polarity, termination, receive behavior, or noise immunity; verify those on the exact bottom-board revision in use.
