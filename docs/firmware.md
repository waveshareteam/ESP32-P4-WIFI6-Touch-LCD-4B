# Firmware and Flash Artifacts

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


Arduino is currently a compile-validation surface. Do not advertise an Arduino
archive as independently flashable until its bootloader, partition table,
application, and exact offsets are captured and verified for the selected
FQBN/options.

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

No complete firmware package was generated or verified for this repository
state, and no on-board flash or hardware test was performed. Ignored exploratory
build output is not release evidence. Package format documentation is a contract
for generated outputs, not evidence that an archive has been produced or verified
for the current commit.

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
