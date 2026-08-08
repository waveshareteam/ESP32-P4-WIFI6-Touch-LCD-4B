# Hello World

This minimal project verifies that the repository, toolchain, and ESP32-P4
target are configured correctly. It does not initialize the display, touch
controller, audio hardware, storage, or wireless coprocessor.

## Build

From the repository root, with an ESP-IDF environment active:

```sh
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

To flash and monitor, replace the serial port with the port used by the board:

```sh
idf.py -C examples/esp-idf/hello_world -p PORT flash monitor
```
