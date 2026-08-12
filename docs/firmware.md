# Firmware and Flash Artifacts

[English](firmware.md) · [简体中文](firmware_ZH.md)

This repository distinguishes source firmware, source-built packages, and
factory or recovery images. These terms are not interchangeable.

## Artifact classes

| Class | Repository location | Meaning |
| --- | --- | --- |
| Source firmware | `firmware/brookesia/` | Maintained ESP-IDF source project for the board |
| Source-built package | Ignored output such as `release-artifacts/` or `releases/dist/` | Reproducible output built from a specific repository revision |
| Factory or recovery image | Not included at present | Vendor-provided image intended for production or recovery, if added later |
| ESP32-C6 slave firmware | Not included at present | Separate coprocessor image that must match the P4 Hosted stack |

The official product archive's `11_esp_brookesia_phone` project is represented
by the maintained `firmware/brookesia/` tree rather than copied as a second
source project. Calling this source “factory firmware” does not make a locally
built binary identical to a vendor-programmed production image.

Factory/recovery images and C6 source/build instructions are not included in
this repository yet and may be added in a later update. Their absence must not
be described as proof that they are closed or permanently unavailable.

## Brookesia source firmware

The Brookesia project currently targets ESP-IDF v5.5.5 and ESP32-P4. It uses a
custom partition table with an application, ESP-SR model image, and SPIFFS
storage image. A complete first installation must use the project-generated
flash arguments so every required image is written at the correct offset.

Brookesia has two incompatible board profiles: `rev1_3` is the default/pre-v3
profile (minimum silicon revision 1.0, maximum exclusive 3.0, 200 MHz PSRAM),
while `rev3_x` has a 3.0 minimum, no claimed validated upper hardware bound, and
the existing 250 MHz PSRAM setting. They use separate SDKCONFIG and build
directories in CI and must never share a binary. The v3.x profile requires
ESP-IDF 5.5.3+ or 6.0+; v5.5.5 meets that software prerequisite but does not
prove hardware compatibility. Confirm the matching PCB/electrical revision too.

```sh
idf.py -C firmware/brookesia -p PORT flash monitor
```

Use `app-flash` only when the partition table, model configuration, and SPIFFS
assets have not changed. It does not update `srmodels.bin`, `storage.bin`, or
the partition table.

Brookesia IDF v6 support is not claimed. The P4/C6 wireless compatibility
contract is documented separately in `p4-c6-hosted-wifi.md`.

## Source-built packages

`scripts/package_esp_idf_firmware.py` packages every image referenced by an
ESP-IDF build directory's `flasher_args.json`. It creates a ZIP containing:

- `manifest.json` with target, framework version, repository-relative project
  path, source revision, timestamp, flash settings, offsets, sizes, and SHA-256 hashes.
- `flash.py`, which validates the package files and command mapping before
  executing the manifest argument array without a shell.
- `flash.sh` and `flash.cmd`, which enter the package directory and invoke the
  Python runner without embedding image names in shell commands.
- `bin/` with every referenced image, preserving nested build paths where
  needed.

Example, after a successful build:

```sh
python scripts/package_esp_idf_firmware.py \
  examples/esp-idf/hello_world/build \
  --output-dir release-artifacts \
  --project hello_world \
  --idf-version v5.5.5
```

The helper requires the Python `esptool` module when a recipient runs the
included flash script. Details are in `../releases/README.md`.

Package directory names include a short source-revision label. Existing
archives are not replaced unless `--overwrite` is provided, and unknown
`extra_esptool_args` fields cause packaging to stop rather than emit an
incomplete flash command.


Arduino CI packages are flashable only when the exact ESP32-P4 FQBN, one accepted
binary layout, every offset, and every hash pass the package gates on an exact-SHA
Actions run. A compile alone is not a package, flash, or runtime-validation
result.

## CI firmware packages and cross-platform flashing

Successful CI builds package a schema-1 ZIP for every matrix item. The package
records the exact full source SHA, board `ESP32-P4-WIFI6-Touch-LCD-4B`,
`esp32p4`, explicit `rev1_3` or `rev3_x` board profile and auditable chip-revision
bounds, 32 MiB flash bound, source project, offsets,
sizes, and SHA-256 values. It contains no ESP32-C6 coprocessor image and the
Windows flasher rejects a manifest that says otherwise.

Use the repository-level wrapper from a clean, non-detached checkout. Windows
uses `Flash-CI-Firmware.cmd` (which forwards through PowerShell); Linux uses
`./Flash-CI-Firmware.sh`. Git, Python with `esptool`, and either authenticated
GitHub CLI or `GH_TOKEN`/`GITHUB_TOKEN` are required. The repository owner/name
is discovered from `origin`, never copied into the command.

```text
Flash-CI-Firmware.cmd -SelfTest
Flash-CI-Firmware.cmd -List
Flash-CI-Firmware.cmd -Preflight
Flash-CI-Firmware.cmd -Item 1 -Port COMx
./Flash-CI-Firmware.sh --self-test
./Flash-CI-Firmware.sh --list
./Flash-CI-Firmware.sh --preflight
./Flash-CI-Firmware.sh --item 1 --port /dev/ttyUSB0
```

`SelfTest` is offline. `List` authenticates and lists only packages available
from complete successful workflow runs at the current branch's exact local HEAD.
`Preflight` checks Git/origin/authentication, Python `esptool`, exact-HEAD runs,
and non-expired non-empty artifact metadata without downloading an artifact or
opening a serial port. Both modes reject incomplete or old-SHA runs.
For each workflow the newest completed/successful exact-HEAD run is the only
candidate: if its artifact set is partial, expired, empty, missing, or duplicate,
the command fails and never falls back to an older run.

Normal use first performs the same preflight, then lets the operator select any
of the dynamically derived 33 artifacts (26 ESP-IDF, five Arduino, two
Brookesia profiles). It downloads into an OS-user cache, verifies the schema-1
manifest, paths, hashes, offsets, capacity, and canonical non-erasing command,
then probes the selected port. Chip major revision below 3 requires `rev1_3`;
3 or above requires `rev3_x`, which still requires an independent PCB/electrical
confirmation. The operator must type exact `FLASH`; one write runs, must report
`Hash of data verified`, and then the program exits. It never auto-advances.

For ESP-IDF packages, each verified manifest file also carries its original
`flasher_args.json` metadata path. The flasher requires that mapping to match
the package's bundled `metadata/flasher_args.json` exactly, so a missing model,
storage, bootloader, partition, or application image is rejected. Arduino
packages must carry the repository CI's exact FQBN and either one merged image
at offset zero or the complete four-image layout.

The local `package_esp_idf_firmware.py` package format and historical local
package examples are separate from the CI schema-1 artifact contract above.

## Factory and recovery images

If authorized vendor images are added later:

- Store them separately from source projects and CI output.
- Record product, main-board revision, bottom-board revision when relevant,
  ESP32-P4 silicon revision, C6 firmware relationship, flash size, offsets,
  version, source URL, retrieval date, and SHA-256.
- Include vendor flashing and recovery instructions without rewriting offsets
  from memory.
- Do not rebuild or repackage them in CI as though they were source-built.
- Preserve their original license and redistribution terms.

## Validation state

This source change and its static checks do not themselves prove that Actions
produced a package, that it was flashed, or that hardware ran correctly. Use the
exact committed SHA's Actions evidence and a named board test record for those
claims. Ignored exploratory build output is not release evidence.

A release record should distinguish:

1. Source commit and exact toolchain versions.
2. Successful compile evidence.
3. Package-content verification against `flasher_args.json`.
4. Flash and runtime results on a named hardware revision.

## Sensitive data

Before distributing a package, verify that it does not contain Wi-Fi
credentials, API keys, certificates, activation identifiers, user data, or
host-local paths. Generated manifests must use repository-relative project
paths and generic commands.
