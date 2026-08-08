# USB Extended Screen

[中文](README_ZH.md)

This ESP-IDF example turns the Waveshare ESP32-P4-WIFI6-Touch-LCD-4B into a
fixed-size 720 x 720 USB auxiliary display. It is based on the Waveshare
product example and the Espressif `usb_extend_screen` implementation, with the
board integration normalized for the published Waveshare BSP 3.0.0.

This is a vendor-specific USB display, not a USB Video Class (UVC) device and
not a standards-based DisplayPort/USB display. A compatible Windows Indirect
Display Driver (IDD) sends compressed frames over a TinyUSB vendor endpoint.

## Data path

```text
Windows desktop
  -> Espressif-compatible IDD
  -> USB 2.0 High-Speed vendor OUT endpoint
  -> bounded JPEG frame queue
  -> ESP32-P4 hardware JPEG decoder
  -> 720 x 720 RGB565 MIPI-DSI frame buffer
```

The default composite device also exposes:

- a HID digitizer report for up to five GT911 contacts; and
- USB Audio Class speaker and microphone interfaces.

HID and UAC can be disabled independently in `menuconfig`. If both are
disabled, the example uses the vendor-only product ID described below.

## Hardware and software scope

- Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-4B.
- Panel: 4-inch ST7703-compatible 720 x 720 MIPI-DSI panel.
- Touch: GT911, with five contacts represented by the HID descriptor.
- USB: the ESP32-P4 native USB 2.0 device/OTG connection, configured for the
  High-Speed UTMI PHY path.
- ESP-IDF source range: `>=5.5,<7.0`.
- Board component: `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.0.
- LVGL: 9.3.0 with `espressif/esp_lvgl_adapter` 0.6.3.
- UAC component: `espressif/usb_device_uac` 1.3.1.
- TinyUSB component: `espressif/tinyusb` `0.19.0~3` (exact).
- Retained optional player helper: `chmorgan/esp-audio-player` 1.0.7, with
  `chmorgan/esp-file-iterator` 1.0.0.

Use a data-capable cable on the native USB port. A serial/JTAG-only connector
cannot carry the vendor display stream.

## Windows IDD requirement

Windows does not treat the vendor interface as a monitor without a matching
IDD. Espressif documents its host-side driver here:

- [USB extended-screen Windows driver instructions](https://github.com/espressif/esp-iot-solution/blob/master/examples/usb/device/usb_extend_screen/windows_driver/README_cn.md)
- [Espressif USB extended-screen example](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_extend_screen)

The upstream instructions currently link this signed installer:

- [xfz1986_usb_graphic_250224_rc_sign.exe](https://dl.espressif.com/AE/esp-iot-solution/xfz1986_usb_graphic_250224_rc_sign.exe)

The installer is an external binary. It is not vendored, mirrored, checksummed,
or redistributed by this repository. Review its publisher signature, origin,
and suitability for the target system before running it. The upstream driver
documentation targets Windows 10/11; this repository does not provide a Linux
or macOS display driver.

### USB identity

| Configuration | VID | PID |
| --- | ---: | ---: |
| Vendor + HID and/or UAC (default) | `0x303A` | `0x2986` |
| Vendor interface only | `0x303A` | `0x2987` |

The IDD configuration must match the firmware VID/PID. Changing HID or UAC can
therefore change the default PID.

## Vendor frame protocol

The host writes a packed 16-byte, little-endian header followed by
`payload_total` payload bytes. The firmware consumes the payload as a stream;
a USB transfer does not have to contain a complete frame.

| Offset | Field | Size | Current device behavior |
| ---: | --- | ---: | --- |
| 0 | `crc16` | 2 bytes | Present for upstream protocol compatibility; not validated by this firmware |
| 2 | `type` | 1 byte | `3` (JPEG) is accepted; RGB565, RGB888, and YUV420 are rejected |
| 3 | `cmd` | 1 byte | Reserved by the imported protocol; not interpreted |
| 4 | `x` | 2 bytes | Must be `0` |
| 6 | `y` | 2 bytes | Must be `0` |
| 8 | `width` | 2 bytes | Must be `720` |
| 10 | `height` | 2 bytes | Must be `720` |
| 12 | `frame_id` | 10 bits | Carried in the packed final word; not used for ordering |
| 12 | `payload_total` | 22 bits | Declared JPEG length; must be 1 to 300000 bytes |

The `UDISP_TYPE_END` value is `0xFF`. The current rendering path accepts only a
full-screen JPEG frame. Partial rectangles and raw RGB/YUV payloads are not
implemented.

The receive path rejects short headers, unexpected geometry, zero-length or
oversized payloads, data beyond the declared payload, and writes beyond the
allocated frame buffer. When no empty frame is available, or a frame type is
unsupported, the remaining declared payload is consumed and dropped so it is
not reinterpreted as a new header.

This framing is still not an authenticated transport. Treat the connected USB
host and its driver as trusted. The header CRC field is not a security or
integrity boundary in the current implementation.

## Build and flash

Activate a supported ESP-IDF environment, then run from this directory:

```sh
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

The checked-in defaults select the 4-inch 720 x 720 panel, RGB565, three DPI
frame buffers, 32 MB flash, PSRAM, USB High Speed, and 48 kHz/16-bit stereo UAC
interfaces. Use `idf.py menuconfig` before building if the connected hardware
or desired USB composition differs.

## Validation boundary

The source has been normalized and statically reviewed. No local build,
flashing, USB enumeration, Windows IDD test, display test, audio test, or touch
test was performed as part of this update. Consult the repository's
[CI status](../../../docs/ci.md) for compile results rather than treating this
README as a build certificate.

The optional player-helper versions above are a static dependency selection
from the imported `bsp_extra` component. They were not resolved or compile
validated during this update and are not used by the USB display data path.

Hardware smoke testing must cover at least:

1. High-Speed enumeration with the selected composite VID/PID.
2. Repeated malformed and oversized vendor frames without queue corruption.
3. Sustained 720 x 720 JPEG updates and visible frame order.
4. Five-contact press, move, release, and contact-ID behavior in Windows.
5. UAC playback, mute, volume, and endpoint stability.
6. UAC microphone capture from the ES7210 path.

The UAC microphone path is specifically unverified. The board combines ES8311
playback with ES7210 capture, while the imported glue initializes the BSP's
default duplex audio path before opening both codecs. Confirm the required
ES7210 TDM slot configuration on hardware before claiming microphone support.
Disabling UAC is the safest configuration when audio is not required.

## Provenance

The exact Waveshare archive, retrieval date, hashes, and import mapping are
recorded in [the repository source record](../../../docs/sources.md). Useful
upstream references are:

- [Waveshare product documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B)
- [Waveshare BSP 3.0.0](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_4b/versions/3.0.0)
- [Espressif USB Device UAC 1.3.1](https://components.espressif.com/components/espressif/usb_device_uac/versions/1.3.1)

Imported provenance does not imply runtime validation or permission to
redistribute third-party binaries.
