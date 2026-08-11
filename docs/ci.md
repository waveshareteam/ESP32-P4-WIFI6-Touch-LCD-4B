# Continuous Integration

[English](ci.md) · [简体中文](ci_ZH.md)

CI separates first-party examples, maintained source firmware, and repository
policy into explicit surfaces. A workflow being present is only a matrix
contract; an exact Actions job on an exact commit is the compile evidence.

## 🧭 Scope and boundaries

- `examples/esp-idf/` contains 13 immediate first-party projects.
- `examples/arduino/` contains 5 immediate first-party sketches. Bundled
  libraries and any library-owned examples are not independent product targets.
- `firmware/brookesia/` is maintained source firmware, not an example. It has a
  separate source-impact/manual workflow and is never auto-discovered by example CI.
- Documentation, governance, and repository templates retain visible policy and
  routing checks without rebuilding product firmware.

This separation prevents a nested upstream demo or a firmware directory from
silently expanding the product example matrix.

## 📌 Version matrix

The stable versions were rechecked on 2026-08-10 and are pinned rather than
referenced through moving aliases:

| Surface | Framework version | Target | Default entries |
| --- | --- | --- | ---: |
| 13 ESP-IDF examples | [ESP-IDF v5.5.5](https://github.com/espressif/esp-idf/releases/tag/v5.5.5) | `esp32p4` | 13 |
| 13 ESP-IDF examples | [ESP-IDF v6.0.2](https://github.com/espressif/esp-idf/releases/tag/v6.0.2) | `esp32p4` | 13 |
| 5 Arduino sketches | [Arduino-ESP32 3.3.11](https://github.com/espressif/arduino-esp32/releases/tag/3.3.11) | `esp32:esp32:esp32p4` | 5 |
| Brookesia firmware | ESP-IDF v5.5.5 | `esp32p4` | 2 isolated profiles: `rev1_3`, `rev3_x` |

A complete build-impacting change therefore produces 26 default ESP-IDF builds
and 5 Arduino compiles. Brookesia runs through two isolated jobs in the separate `Firmware Build`
workflow when its source/resources or workflow change, and can also be selected
manually. IDF v6 support for that maintained firmware remains pending and must
not be inferred from the example matrix.

Maintainers must recheck stable upstream releases before updating a pin. Beta,
RC, alpha, and preview releases do not belong in the default matrix.

## 🔀 Changed-file routing

Both default build workflows run a lightweight `discover` job for every pull
request and every push to `main`. `scripts/select_ci_targets.py` consumes the complete Git
name-status scope, including deleted paths and both sides of renames, then
applies these rules:

| Change class | ESP-IDF result | Arduino result |
| --- | --- | --- |
| Markdown, schematics, drawings, screenshots, governance, templates | No build | No build |
| Source/configuration below one first-party example | That example on both pinned IDF versions | That sketch |
| `config/` shared defaults | All 13 projects on both versions | No build |
| Product Arduino helper library | No build | All 5 sketches |
| Framework workflow definition | Complete affected framework matrix | Complete affected framework matrix |
| Selector, CI flasher, packager, or their tests | All 26 builds | All 5 compiles |
| `firmware/` source, media, archive, or binary | Report firmware touched; no example build | Report firmware touched; no example build |
| Unknown non-documentation input | All 26 builds | All 5 compiles |

An empty changed-file input is an operational error, not a successful no-build
decision. The conservative fallback protects new shared files until maintainers
add a narrower rule.

Manual dispatch accepts `all`, a target basename such as `hello_world` or
`LVGLV9_Arduino`, or an exact repository-relative example path. Unknown input
fails rather than creating an empty green run. Brookesia is selected through
its own firmware workflow, not through an example selector.

## 🧱 ESP-IDF environment

The shared defaults make every example a `rev1_3` pre-v3 target with
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_100=y`.
There is no ESP-IDF “1.3-only” Kconfig symbol. Arduino remains
`ChipVariant=prev3`. Examples retain the 26/5 matrix rather than being doubled.
Only Brookesia has `rev1_3` (200 MHz PSRAM) and `rev3_x` (250 MHz PSRAM)
profiles, each with its own build directory and generated sdkconfig. The
profiles are software-incompatible; v3.x needs ESP-IDF 5.5.3+ or 6.0+.

Each matrix job runs the official container for its pinned IDF version, sources
`$IDF_PATH/export.sh`, selects `esp32p4`, resolves managed components in a clean
checkout, and builds the selected project. The export step is required because
GitHub's job-container wrapper replaces the image entry point; without it,
`idf.py` is not on `PATH`.

Third-party Actions are pinned to reviewed commit SHAs, and the IDF v5.5.5 and
v6.0.2 containers are pinned to the image digests observed for this migration.
Updating a framework version therefore requires an explicit digest review.

Generated `managed_components/`, `sdkconfig`, `dependencies.lock`, and `build/`
directories are not source inputs. Jobs must not depend on copies produced by a
developer machine.

The board migration baseline is the managed
`waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.0 BSP. The hosted Wi-Fi example pins
`esp_wifi_remote` 1.6.3 with `esp_hosted` 2.12.11. A successful host compile
does not prove compatibility with the ESP32-C6 image installed on a board.

The BSP 3.0.0 LVGL projects constrain `esp_lvgl_adapter` to 0.6.3. LVGL 9
projects use 9.3.0 because adapter 0.6.3 consumes APIs introduced in that
release; the LVGL 8 project retains 8.4.0 and supplies a compile-only alias for
the BSP's LVGL 9 rotation type spelling. The LVGL 9 projects disable optional
fast-memory IRAM placement because GCC 15 rejects the differing generated
section attributes on repeated LVGL 9 declarations. The USB composite project
also sets `USB_DEVICE_UAC_AS_PART` so its application-owned TinyUSB descriptor
is not duplicated by the UAC component.

## 🔧 Arduino environment

Arduino jobs use:

- Core `esp32:esp32@3.3.11` and FQBN `esp32:esp32:esp32p4`.
- `FlashMode=qio`, `FlashSize=32M`, `PSRAM=enabled`,
  `PartitionScheme=app13M_data7M_32MB`, and `ChipVariant=prev3`.
- Default USB, CDC-on-boot, and upload-mode values so the separate CH343P debug
  UART is not replaced by native USB CDC.
- The product helper under `examples/arduino/libraries/`.
- GFX Library for Arduino 1.6.6 and LVGL 9.3.0 from Arduino Library Manager.

The LVGL sketch uses only public core LVGL APIs; it does not depend on the
upstream repository's non-library `demos/` directory. Arduino compilation does
not validate the GT911, display timing, or ESP32-C6 Wi-Fi runtime.

## ✅ Policy and evidence

`Repository Policy` runs a repository-local checker plus standard-library unit
tests. The checker validates local links and fragments, the complete maintained
English/Simplified-Chinese document surface, reciprocal language navigation,
same-language local routing, public-text privacy, and CI boundaries. The tests
also cover the expected 13/5 inventory, documentation-only changes, direct and
shared inputs, firmware boundaries, unknown-input fallback, renames, copies,
deletions, empty scope, and the exact manual selector invocation. Synthetic
packaging tests verify complete flash-file capture, manifest hashes and
ordering, unsafe-path rejection, and overwrite protection; none of these
policy checks compile firmware.

Use evidence terms consistently:

| Status | Meaning |
| --- | --- |
| Matrix target | The project/version pair is intended to run in CI |
| Compile passed | An Actions job completed successfully for an exact commit and framework version |
| Package verified | An archive manifest matched the build's flash arguments and files |
| Hardware verified | The named example was flashed and exercised on a stated board revision |

Do not convert compile success into a claim about display, touch, audio,
storage, USB, C6, Ethernet, relay, or RS485 behavior. Ignored local build trees
are not validation evidence.

## 📦 Firmware packages

After a successful build, each matrix job packages and uploads one CI ZIP for
14 days: `firmware-esp-idf-<name>-<idf-version>-rev1_3`,
`firmware-arduino-<name>-3.3.11-rev1_3`, or
`firmware-brookesia-v5.5.5-rev1_3` / `firmware-brookesia-v5.5.5-rev3_x`. The package
uses the final pull-request SHA (or push SHA), and the workflow fails if the ZIP
is absent. ESP-IDF packaging derives every image and offset from
`flasher_args.json`; Brookesia therefore includes its model and filesystem
images. Arduino packaging accepts exactly one merged image or one unambiguous
bootloader/partition/OTA/application layout for the selected 32 MiB pre-v3 FQBN.
Manifest profile and chip-revision bounds are checked before flashing; a chip
major revision below 3 accepts only `rev1_3`, and major revision 3 or later only
accepts `rev3_x`. For v3.x, matching PCB/electrical revision remains mandatory.

`Flash-CI-Firmware.cmd` is the Windows sequential manual-test entry point. It
will not use stale SHA artifacts, a dirty/detached checkout, a draft/missing PR,
or an unverified package. Compile/package success proves neither a flash nor a
manual runtime result; the operator records PASS only after the post-flash test.

CI-built packages are not factory or recovery images and never contain or flash
an ESP32-C6 coprocessor image. Hardware PDFs, drawings, and imported resource
archives must never be included in a firmware artifact. Generated packages remain
under ignored paths such as `release-artifacts/` or `releases/dist/`.

## 🧰 Manual dispatch and reproduction

The default workflow entry points are:

- `ESP-IDF Build` with an optional example selector.
- `Arduino Build` with an optional sketch selector.
- `Firmware Build` for source-impact or manual validation of the explicit Brookesia boundary.

For developer reproduction, activate the exact framework version before using
the same project command:

```sh
idf.py --version
idf.py -C examples/esp-idf/hello_world set-target esp32p4
idf.py -C examples/esp-idf/hello_world build
```

Arduino commands and board options are documented in
`examples/arduino/README.md`. Record evidence with repository-relative paths;
never publish machine-specific tool locations, usernames, credentials, network
names, or private device details.
