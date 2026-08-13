# ESP-Brookesia Factory Firmware

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5.5-E7352C?logo=espressif)](https://github.com/espressif/esp-idf/releases/tag/v5.5.5)
[![Target](https://img.shields.io/badge/target-ESP32--P4-1F6FEB)](https://www.espressif.com/en/products/socs/esp32-p4)
[![LVGL](https://img.shields.io/badge/LVGL-9.4.0-2AA9E0?logo=lvgl)](https://lvgl.io/)

English | [简体中文](README_CN.md)

This directory contains the source factory firmware for the
**Waveshare ESP32-P4-WIFI6-Touch-LCD-4B**. It turns the 720 x 720 touch display
into an ESP-Brookesia application shell and integrates the board display,
touch, audio, camera, SD card, Ethernet, relays, RS485, and ESP32-C6 hosted
Wi-Fi paths.

> [!IMPORTANT]
> This is board-specific firmware, not a generic ESP-Brookesia example. Use
> ESP-IDF v5.5.5 and the pinned component manifests in this project. The
> ESP32-P4 target, partition table, BSP revision, and C6 hosted firmware form a
> compatibility set and should be changed together only after hardware testing.

## Platform baseline

| Layer | Current project baseline |
| --- | --- |
| Host MCU | ESP32-P4, 32 MB flash, 32 MB PSRAM |
| Display / touch | 4-inch 720 x 720 MIPI DSI ST7703 / GT911 |
| Framework | ESP-IDF v5.5.5 (`idf_component.yml` requires `>=5.5,<6.0`) |
| UI | LVGL 9.4.0 with local ESP-Brookesia core 0.6.0-beta2 |
| Board component | Vendored `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.0 snapshot; project overlay constrains the Registry identity to 3.0.0 |
| LVGL integration | `espressif/esp_lvgl_adapter` 0.6.2, not `esp_lvgl_port` |
| Wireless | ESP32-C6 coprocessor over SDIO using ESP-Hosted / `esp_wifi_remote` |
| Voice assistant | `espressif/esp_xiaozhi` 0.1.1, ESP-SR 2.4.7, Xiaozhi fonts 1.6.0 |

The vendored BSP and ST7703 sources preserve the factory-firmware integration
state and therefore shadow managed components of the same name. Standalone
examples use the published BSP 3.0.0. Remove the vendored copies only after the
complete Brookesia firmware has been compared and hardware-tested against the
Registry release.

Vendored upstream manifests retain their compatibility ranges. The active
ESP-IDF v5.5.5 graph is narrowed by exact project/component constraints. The
generated `dependencies.lock` remains ignored because local component entries
contain host-specific paths; record resolved transitive versions with CI evidence.

Hardware pin details and the P4/C6 compatibility notes live in
[`docs/board.md`](../../docs/board.md) and
[`docs/p4-c6-hosted-wifi.md`](../../docs/p4-c6-hosted-wifi.md).

## Hardware revision profiles

The default profile is `rev1_3`: pre-v3 ESP32-P4 with
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`, `CONFIG_ESP32P4_REV_MIN_100=y`, and the
200 MHz PSRAM baseline. `rev3_x` explicitly selects
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n`, has a 3.0 minimum, and retains 250 MHz
PSRAM. They are separate, incompatible binaries; do not flash either profile to
the other silicon range. CI uses `sdkconfig.defaults.rev1_3` or
`sdkconfig.defaults.rev3_x` with independent SDKCONFIG/build paths. The v3.x
profile needs ESP-IDF 5.5.3+ or 6.0+, but a successful compile does not prove
the matching PCB/electrical revision or hardware operation.

## Applications

The launcher installs the following applications. The PNGs below are the same
source icons used to generate the compiled LVGL assets.

<table>
  <tr>
    <td align="center"><img src="components/XiaozhiApp/assets/img_app_xiaozhi.png" width="64" alt="AIChats icon"><br><b>AIChats</b></td>
    <td align="center"><img src="components/brookesia_app_calculator/assets/img_app_calculator.png" width="64" alt="Calculator icon"><br><b>Calculator</b></td>
    <td align="center"><img src="components/draw/assets/img_app_drawpanel.png" width="64" alt="DrawPanel icon"><br><b>DrawPanel</b></td>
    <td align="center"><img src="components/SpecAnalyzer/assets/img_app_specanalyzer.png" width="64" alt="SpecAnalyzer icon"><br><b>SpecAnalyzer</b></td>
    <td align="center"><img src="components/MusicPlayer/assets/img_app_musicplayer.png" width="64" alt="MusicPlayer icon"><br><b>MusicPlayer</b></td>
    <td align="center"><img src="components/Settings/assets/img_app_settings.png" width="64" alt="Settings icon"><br><b>Settings</b></td>
  </tr>
  <tr>
    <td align="center"><img src="components/Camera/assets/img_app_camera.png" width="64" alt="Camera icon"><br><b>Camera</b></td>
    <td align="center"><img src="components/VideoPlayer/assets/img_app_vedioplayer.png" width="64" alt="VideoPlayer icon"><br><b>VideoPlayer</b></td>
    <td align="center"><img src="components/HardwareApps/assets/img_app_ethernet.png" width="64" alt="Ethernet icon"><br><b>Ethernet</b></td>
    <td align="center"><img src="components/HardwareApps/assets/img_app_relay_control.png" width="64" alt="Relay Control icon"><br><b>Relay Control</b></td>
    <td align="center"><img src="components/HardwareApps/assets/img_app_rs485.png" width="64" alt="RS485 icon"><br><b>RS485</b></td>
    <td align="center"><img src="components/brookesia_app_squareline_demo/assets/esp_brookesia_app_icon_launcher_squareline_112_112.png" width="64" alt="Squareline icon"><br><b>Squareline</b></td>
  </tr>
</table>

| Application | What it demonstrates | Runtime dependency |
| --- | --- | --- |
| AIChats | Network check, Xiaozhi activation, WakeNet wake word, VAD, Opus uplink/downlink, server transcript and TTS | Wi-Fi or Ethernet, `model` and `storage` images, ES7210/ES8311 |
| Calculator | Touch-friendly calculator built as a Brookesia phone app | None beyond display/touch |
| DrawPanel | Drawing in a fixed canvas region without dragging the entire app at the edge | Display/touch |
| SpecAnalyzer | Two independent FFT views from the two front microphones | ES7210 TDM input |
| MusicPlayer | Up to five locally supplied MP3 tracks with reusable cover/spectrum artwork | SPIFFS `storage`, ES8311 output |
| Settings | WLAN scan/connect, sound, display brightness, and device information pages | ESP32-C6 hosted Wi-Fi and board services |
| Squareline | Registry-installed Squareline Studio UI demo and application template | Display/touch |
| Camera | Full-screen MIPI CSI preview with PPA crop/scale | Supported OV5647 sensor mode and `esp_video` |
| VideoPlayer | Full-screen SD-card AVI/MJPEG playback with audio when available | SD card, JPEG/PPA, ES8311 output |
| Ethernet | IP101 link, IPv4, netmask, gateway, MAC, speed/duplex, and PHY status | IP101 plus matching 86-panel bottom-board RJ45 path |
| Relay Control | Independent active-high control of GPIO32 and GPIO46 with NVS state restore | Relay outputs and NVS |
| RS485 | Periodic transmit and receive terminal with counters and a clear action | UART1 and board transceiver |

## Build

### Prerequisites

- ESP-IDF v5.5.5 with the `esp32p4` toolchain installed.
- Git and network access to the ESP Component Registry and the pinned Waveshare
  component repository on the first dependency resolution.
- An ESP32-P4-WIFI6-Touch-LCD-4B and a data-capable USB cable for flashing.
- Enough disk space for managed components and the generated SPIFFS/model
  images.

Activate the intended ESP-IDF environment first and verify it explicitly:

```sh
idf.py --version
idf.py -C firmware/brookesia set-target esp32p4
idf.py -C firmware/brookesia build
```

The checked-in `sdkconfig.defaults` selects the ESP32-P4 target, 32 MB flash,
hex PSRAM, the 720 x 720 panel, OV5647 camera mode, ESP32-C6 hosted transport,
and the "Ni Hao Xiao Zhi" WakeNet model. Do not edit files under
`managed_components/`; update a component manifest and let the IDF Component
Manager regenerate them.

With local components, the generated `dependencies.lock` contains absolute
component paths from the current build host. It is therefore ignored and is a
local build aid, not a publication artifact. Run `idf.py reconfigure` (or the
first `build`) to generate it in a new environment. Keep reproducible BSP
commits and component ranges in `idf_component.yml`, and do not publish a
host-specific lock file.

### First flash and partition changes

The first installation must write the bootloader, partition table, application,
ESP-SR model, and SPIFFS resources:

```sh
idf.py -C firmware/brookesia -p PORT flash monitor
```

Use a full erase before flashing when moving partition offsets/sizes, changing
from an older layout, or investigating a stale model/filesystem image:

```sh
idf.py -C firmware/brookesia -p PORT erase-flash flash monitor
```

After the partition layout, ESP-SR model, and `spiffs/` assets are already on
the device, application-only iterations are faster:

```sh
idf.py -C firmware/brookesia -p PORT app-flash monitor
```

`app-flash` does **not** update `srmodels.bin`, `storage.bin`, or the partition
table. Return to `flash` after changing `partitions.csv`, WakeNet/model Kconfig,
the Xiaozhi font component, bundled music, or prompt audio.

## Flash layout and resources

The current table intentionally ends at 16 MiB even though the board has 32 MB
flash. This keeps full-flash data smaller and keeps the memory-mapped ESP-SR
image in the first 16 MiB while Quad Flash 32-bit addressing is disabled.

| Partition | Offset | Size | Purpose |
| --- | ---: | ---: | --- |
| `nvsfactory` | `0x009000` | 200 KiB | Factory/configuration NVS |
| `nvs` | `0x03B000` | 840 KiB | Runtime NVS, including app state |
| `otadata` | `0x10D000` | 8 KiB | OTA selection metadata |
| `phy_init` | `0x10F000` | 4 KiB | PHY initialization data |
| `model` | `0x110000` | 960 KiB | Generated ESP-SR `srmodels.bin` |
| `factory` | `0x200000` | 8 MiB | Main firmware image |
| `storage` | `0xA00000` | 6 MiB | Generated SPIFFS resources |

Do not move `model` behind a large `factory` or `storage` partition without
also validating the platform's flash-MMAP and 32-bit-address configuration.
Simply detecting 32 MB flash does not make a partition above 16 MiB readable by
every memory-mapped consumer.

`storage.bin` is generated from `spiffs/` and also stages the 30 px common font
from `78/xiaozhi-fonts` as `/spiffs/xiaozhi/font.bin`. Runtime resources include:

- Up to five locally supplied MusicPlayer MP3 files under `/spiffs/music`; these remain ignored until redistributable assets and licenses are selected.
- Xiaozhi digit, activation, and success OGG prompts under `/spiffs/xiaozhi`.
- The Xiaozhi common Chinese/Latin font loaded into PSRAM at runtime.

## AIChats / Xiaozhi

AIChats is the launcher name of `components/XiaozhiApp`. It uses the managed
`esp_xiaozhi` protocol component while retaining board-specific UI, audio,
activation, and lifecycle integration.

### First-use flow

1. Open AIChats. Its UI is created immediately while readiness checks continue
   in the background.
2. The app accepts either a valid station IPv4 address or an Ethernet IPv4
   address. It does not run its own provisioning portal; configure WLAN in
   Settings or connect Ethernet.
3. If the device is not associated with a Xiaozhi account, the activation code
   is shown and announced. Add the device in the Xiaozhi control panel and
   enter that code.
4. After successful activation, the local success prompt is played and the
   transport connects.
5. When the UI reaches standby, say the configured wake phrase (the default
   model is "Ni Hao Xiao Zhi") and continue the conversation after the reply.

If the log reports `No speech model found` or `contains no compatible models`,
rebuild and perform a full flash so the partition table and
`srmodels/srmodels.bin` are both written. Application-only flashing cannot fix
this condition.

### Compile-time UI language

The local status text defaults to English. Change it with:

```text
idf.py menuconfig
  Xiaozhi Application
    Local UI language
      English
      Simplified Chinese
```

The corresponding options are
`CONFIG_XIAOZHI_APP_UI_LANGUAGE_ENGLISH` and
`CONFIG_XIAOZHI_APP_UI_LANGUAGE_SIMPLIFIED_CHINESE`. This setting affects only
firmware-owned status text and fallback activation instructions. Conversation
text, TTS, and messages returned by the server are displayed or played as
received; selecting the UI language does not translate the service response.

To change the maintained product default, replace the English symbol in
`sdkconfig.defaults` with the Simplified Chinese symbol and keep only one
choice set to `y`. Existing build directories retain their generated
`sdkconfig`; use `menuconfig` or regenerate `sdkconfig` after changing the
defaults file.

Add or revise local strings in `components/XiaozhiApp/XiaozhiLocalization.*`.
Keep server-provided transcript and activation text out of that table.

### Product tool extension hook

`registerXiaozhiDeveloperTools()` in
`components/XiaozhiApp/XiaozhiDeveloperTools.cpp` is the intended MCP extension
point. It runs for each new MCP engine before transport startup and currently
registers no tools. The TODO documents candidate product tools for:

- Setting display brightness.
- Setting output volume.
- Capturing a camera frame for a vision request.

Implement only callable tools: create the MCP schema and callback, validate
arguments, add the tool to the engine, and report the real operation result.
Do not advertise placeholder tools through `tools/list`. Dispatch blocking
hardware work to an application task rather than blocking the protocol
callback, and retain/copy camera buffers until the result builder has consumed
them.

### Voice audio contract

AIChats configures the board audio bus at 24 kHz, extracts two microphone
channels and one echo-reference channel, resamples them to 16 kHz, and feeds
ESP-SR in `MMR` order. The Opus uplink is 16 kHz; server playback is resampled
to the 24 kHz device rate when required.

| ES7210 serialized TDM slot | Board-observed source | Use |
| ---: | --- | --- |
| 0 | Physical MIC1 | SpecAnalyzer left; AIChats microphone 1 |
| 1 | Physical MIC3 | AIChats echo reference |
| 2 | Physical MIC2 | SpecAnalyzer right; AIChats microphone 2 |
| 3 | Physical MIC4 | Captured in the full frame but not used by these apps |

TDM slot masks and physical ES7210 microphone gain masks are different
namespaces. Never pass a serialized slot mask as a physical microphone mask.
SpecAnalyzer deliberately captures all four serialized slots and extracts
slots 0 and 2 in software; its diagnostic log prints
`TDM peaks [MIC1, MIC3(echo), MIC2, MIC4]`.

## Hardware application notes

### Relay persistence

Relay Control stores a two-bit state in NVS namespace `relay_control`, key
`output_state`. App initialization restores GPIO32/GPIO46 after a reboot, and
opening the UI reads the current GPIO state so killing and reopening the app
does not reset the switches. Erasing NVS intentionally returns both outputs to
the default off state.

### Ethernet

The Ethernet app starts the IP101 (PHY address 1) with DHCP and shows
link state, IPv4 address, netmask, gateway, MAC address, speed/duplex, and PHY
identity. The pin assignment is project-specific glue in `components/bsp_extra`
and is documented in [`docs/board.md`](../../docs/board.md).

### RS485

The terminal uses UART1 at 115200 8N1 on TX GPIO47 / RX GPIO48. Auto-send is
enabled by default and transmits once per second:

```text
Hi,this is Waveshare ESP32-P4-WIFI6-Touch-LCD-4B!
```

The receive window escapes non-printable bytes and keeps byte/frame counters.
The current product documentation describes automatic direction control. The
reviewed historical baseboard V1.3 schematic instead ties `/RE` and `DE`
together and pulls the net high, so receive may be unavailable on that specific
revision. Do not generalize the V1.3 limitation to the current board. Never
short A and B together for loopback testing; that collapses the differential
bus.

## Camera and media requirements

### Camera

The current defaults select OV5647 MIPI RAW8 800 x 800 at 50 FPS. Camera opens
the MIPI CSI V4L2 device, keeps the sensor node's native width/height, requests
only a compatible LCD color format when needed, and uses PPA to center-crop and
scale into the 720 x 720 display. Do not force `VIDIOC_S_FMT` to 720 x 720 or
another display size if the CSI sensor mode rejects dimension changes.

### VideoPlayer

VideoPlayer mounts the SD card and scans exactly `/sdcard/avi`. Filenames are
otherwise unrestricted and the `.avi` extension is matched case-insensitively.
MP4 files and AVI files in the SD root are not enumerated. The player decodes
AVI/MJPEG video to the full display, loops through discovered files, and sends
AVI audio to the board codec when audio initialization succeeds; otherwise it
continues video-only.

### MusicPlayer

MusicPlayer enumerates up to five locally supplied MP3 files under
`/spiffs/music`. The UI reuses three cover/spectrum sets across those tracks.
The demo MP3 files are ignored until redistributable assets and licenses are
selected, so a clean public checkout may have an empty playlist. Before
release, add approved audio or document how integrators provide it. Replacing
or adding tracks requires rebuilding and flashing `storage.bin`; `app-flash`
alone is insufficient.

## Project structure

| Path | Ownership |
| --- | --- |
| `main/main.cpp` | Board startup, loading UI, phone shell, app installation, status events |
| `components/brookesia_core/` | Local Brookesia application framework and phone UI |
| `components/bsp_extra/` | Product-specific audio, backlight, relay, IP101, and RS485 glue |
| `components/XiaozhiApp/` | AIChats UI, activation, audio pipeline, protocol adapter, MCP hook |
| `components/HardwareApps/` | Relay, Ethernet, and RS485 applications |
| `components/<App>/` | Individual launcher applications and compiled assets |
| `spiffs/` | Source files staged into the `storage` partition image |
| `archive/` | Inactive historical implementation retained for reference; not installed |
| `partitions.csv` | Factory firmware flash layout |
| `sdkconfig.defaults` | Shared maintained project defaults |
| `sdkconfig.defaults.rev1_3` / `.rev3_x` | Mutually exclusive silicon/PSRAM overlays |
| `managed_components/` | Generated Component Manager checkout; do not edit directly |
| `build/` | Generated output; never treat as source |

## Adding an application

Use a small existing component such as `brookesia_app_calculator` as the basic
lifecycle example, and `HardwareApps` or `XiaozhiApp` when background tasks and
shared hardware are involved.

1. Create `components/<AppName>/` with `CMakeLists.txt`, `idf_component.yml`,
   headers, implementation, and an `assets/` directory.
2. Derive from `esp_brookesia::systems::phone::App` and implement the required
   `run()` and `back()` methods. Override `init()`, `deinit()`, `close()`,
   `pause()`, `resume()`, and `cleanResource()` only as needed for resources
   owned by the application.
3. Keep LVGL object creation/mutation on the LVGL thread or within a short
   display lock. Publish data from worker tasks and let a timer/async callback
   update the UI.
4. Provide the compiled LVGL icon and keep its source PNG beside it so launcher
   artwork and documentation remain reviewable.
5. Add required managed dependencies to the component manifest instead of
   copying third-party components.
6. Request the singleton and install it with `phone->installApp()` in
   `main/main.cpp`, or use the existing registry mechanism for a registry-owned
   app.
7. Test repeated open, background, resume, close, and reboot cycles. Release
   every timer, task, queue, codec session, SD handle, and LVGL object owned by
   the app.

## Common pitfalls

- **Long LVGL lock scopes freeze animation.** Hold the display/LVGL lock only
  around UI operations. Do networking, SD scans, codec startup, NVS access,
  model loading, and app construction outside it.
- **A successful app build does not update data images.** Model, font, prompt,
  music, and partition changes require a full `flash`.
- **The model partition is placement-sensitive.** Keep the ESP-SR image below
  the current 16 MiB addressing boundary unless flash-MMAP/32-bit addressing is
  enabled and verified on this board.
- **ES7210 labels are not DMA slot numbers.** Use the observed
  `[MIC1, MIC3(echo), MIC2, MIC4]` serialized order and separate physical gain
  masks from TDM slot masks.
- **Audio apps share one physical bus.** Stop or hand off codec sessions when
  switching between voice capture, music, and video; do not independently
  reconfigure paired I2S channels.
- **Camera dimensions come from the sensor mode.** Scale/crop for the display;
  do not assume the CSI node accepts the LCD dimensions.
- **VideoPlayer is AVI-specific.** Use `/sdcard/avi/*.avi`; MP4 is not supported
  by the current enumerator/player.
- **P4 has no native Wi-Fi radio.** Host components must remain compatible with
  the firmware running on the ESP32-C6 coprocessor. A host-only dependency
  upgrade can break wireless operation even when it compiles.
- **The BSP has a vendored 3.0.0 snapshot.** It shadows the published managed
  component so the imported Brookesia integration remains reproducible. Compare
  the complete firmware with the Registry release before replacing or updating
  it, especially around the `esp_lvgl_adapter` contract.
- **RS485 receive is hardware-revision dependent.** Check transceiver direction
  wiring before diagnosing the terminal software.

## Diagnostics

The periodic SRAM/PSRAM report is compiled out by default through
`EXAMPLE_SHOW_MEM_INFO = false` in `main/main.cpp`. Temporarily enable it only
when investigating memory pressure; the surrounding diagnostic logic remains
available.

Useful runtime evidence includes the first real error, the partition/model
load lines, camera V4L2 format, Ethernet link events, and the four ES7210 TDM
peaks. Include those lines, the board revision, ESP-IDF version, and the exact
reproduction sequence in bug reports.

## Contributing, support, and release checklist

Read the repository-level [contribution guide](../../CONTRIBUTING.md) and
[support policy](../../SUPPORT.md) before opening a change. Keep reusable BSP
or driver fixes in the appropriate upstream component where possible; keep
only product composition and hardware-specific glue in this firmware. The
repository does not currently advertise a verified private vulnerability-
reporting endpoint, so do not place undisclosed security details in a public
issue.

The local Brookesia core retains its
[Apache-2.0 license](components/brookesia_core/license.txt). That license applies
to that component; it does not select a license for the repository as a whole.

Before publishing a release:

- Build from a clean checkout with the intended ESP-IDF version and regenerate
  dependencies from the committed component manifests.
- Full-flash and verify display/touch, both microphones, speaker, Wi-Fi,
  Ethernet, camera, SD/video, relays, RS485 constraints, and Xiaozhi activation
  plus a complete wake/conversation/stop cycle.
- Record the compatible ESP32-C6 hosted firmware revision.
- Package the complete flash set, not only the application binary.
- Remove generated build output and inspect public text for machine-specific
  paths, usernames, credentials, activation identifiers, and Wi-Fi secrets.
- Select and add a repository-level `LICENSE.txt` before public distribution,
  then retain all component license files, SPDX headers, and required
  third-party notices. Until that license is added, do not assume the entire
  repository is covered by any one component's license.
