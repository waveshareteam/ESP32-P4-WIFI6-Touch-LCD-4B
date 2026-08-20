# Firmware Packages

[English](README.md) · [简体中文](README_ZH.md)

This directory documents generated firmware packages. Generated archives are
not committed; the default output locations `release-artifacts/`,
`releases/dist/`, and `releases/downloads/` are ignored.

No release-package generation, flash, or hardware test is recorded for this
repository state. Existing ignored local build directories are not release
evidence.

Successful CI jobs now create temporary downloadable packages for 14 days. They
are traceable CI artifacts, not released factory or recovery images.

## ESP-IDF package creation

After a successful ESP-IDF build, run the repository helper against that
build directory:

```sh
python scripts/package_esp_idf_firmware.py \
  examples/esp-idf/hello_world/build \
  --output-dir release-artifacts \
  --project hello_world \
  --idf-version v5.5.5
```

Useful optional arguments are:

- `--git-sha` to record the source revision when it cannot be detected from the
  CI environment.
- `--baud` to change the default flash baud from 460800.
- `--overwrite` to explicitly replace an archive with the same project,
  framework, and revision label; replacement is rejected by default.

The helper reads `flasher_args.json`; it does not maintain a second hardcoded
offset table.

## Archive layout

Each ZIP contains one top-level package directory:

```text
<project>-<target>-<idf-version>-<git-sha>/
├── manifest.json
├── flash.py
├── flash.sh
├── flash.cmd
└── bin/
    └── files referenced by flasher_args.json
```

`manifest.json` records:

- Package format version.
- Project, repository-relative project path, and project version where available.
- ESP-IDF version and target.
- Source Git SHA.
- UTC creation time and default baud.
- Flash settings and esptool reset options.
- Every file's offset, package path, size, and SHA-256.
- The complete `python -m esptool` command represented as an argument array.

Nested paths under the ESP-IDF build directory are preserved below `bin/` so
same-named images do not overwrite one another.

The revision suffix and default overwrite refusal prevent two different source
states from silently replacing one another under the same archive name.

## Flash a package

Inspect `manifest.json` before flashing. `flash.py` verifies every packaged
file's safe path, offset, size, and SHA-256, then confirms that the validated
file list matches the command before it invokes esptool. The host needs Python
and the `esptool` module.


Both platform wrappers invoke `flash.py`, which passes the manifest argument
array directly to Python without shell interpolation. The packager rejects
unknown ESP-IDF global esptool fields instead of silently dropping them.
On a POSIX shell:

```sh
sh flash.sh --port PORT
```

On Windows:

```bat
flash.cmd --port PORT
```

The generated scripts accept exactly one `--port PORT` pair and reject missing,
extra, or option-shaped arguments. The package validator regenerates both
helpers from the verified metadata plan and compares their bytes, including the
port parser, write options, ordered offsets, quoted filenames, and command.

A Brookesia package must include every image referenced by its generated flash
arguments, including model and SPIFFS images. An application-only binary is
not a complete first-install package.

## Retrieve a CI artifact

The ESP-IDF, Arduino, and Brookesia workflows package after a successful build
and upload one ZIP with `if-no-files-found: error` and 14-day retention. Artifact
names are `firmware-esp-idf-<name>-<idf-version>-rev3_x`,
`firmware-arduino-<name>-3.3.11-rev3_x`, and
`firmware-brookesia-v5.5.5-rev1_3` / `firmware-brookesia-v5.5.5-rev3_x`. The manifest
must carry the complete final PR/push SHA; it is not valid evidence for another
revision.

When the workflow uploads a package, it can be downloaded with the GitHub web
interface or GitHub CLI:

```sh
gh run download RUN_ID --name ARTIFACT_NAME --dir releases/downloads
```

Record the workflow run, source commit, framework version, and artifact SHA-256
when promoting a CI artifact to a release.

## Revision profiles

All example and Arduino packages are `rev3_x`/post-v3 only; the example matrices
remain 26 ESP-IDF builds and five Arduino builds rather than being doubled.
Brookesia is the only maintained product firmware with two profile-safe
artifacts. `rev1_3` is pre-v3 with a 1.00 minimum and 200 MHz PSRAM baseline;
`rev3_x` is post-v3 with a 3.00 minimum and 250 MHz PSRAM baseline. Never
cross-flash the profiles. They are silicon/configuration profiles: the available
main-board schematics do not establish a PCB/electrical difference between them.

## Arduino boundary

Arduino packages require the exact 32 MiB post-v3 FQBN with `FlashSize=32M`,
`ChipVariant=postv3`, and `EraseFlash=none`. Compilation uses `--build-path` so
Arduino-ESP32 3.3.11 leaves both `build.options.json` and `flash_args`. The
packager checks the former's exact FQBN and core path, but excludes it because it
contains host paths. The workflow/local command dynamically maps repository,
Arduino data/user, and temporary roots for C, C++, and assembly without
overriding board flags; every packaged segment is then scanned for private
paths. A canonical whitelisted identity records the raw build-options
filename/size/SHA-256 and is bound by its own manifest hash. Only `flash_args`
supplies safe flash options, offsets, and filenames. The release ZIP contains each referenced bootloader, partition table,
optional `boot_app0`, application, and other toolchain segment; it never ships a
16/32 MiB merged or whole-flash primary image.

Each segment records its source metadata path, role, size, SHA-256, target,
FQBN, framework version, product Git SHA, and exact BSP version/source-commit/
component-tree evidence. `metadata/flash_args` is bundled with its own size and
hash. The package gate checks path safety, symlinks, duplicate offsets,
non-overlap, capacity, archived bytes, metadata identity, and that total segment
bytes are no more than half of the full 32 MiB flash. Manifest
`segmented_bytes` and `segmented_payload_total` both equal that exact sum.
Offsets are never inferred from a
chip family; a real bootloader offset of zero remains valid when generated
metadata declares it.

## Cross-platform CI flasher

Use `Flash-CI-Firmware.cmd` on Windows or `./Flash-CI-Firmware.sh` on Linux.
Both forward to the same Python core and require Git, Python `esptool`, plus
authenticated GitHub CLI or `GH_TOKEN`/`GITHUB_TOKEN`. `origin` identifies the
repository. `--self-test` is offline; `--list` and `--preflight` require
complete, successful, non-expired artifacts from the exact local HEAD but do not
download or probe hardware. `--preflight` also verifies the local `esptool`
import. Windows accepts equivalent `-SelfTest`, `-List`/`-ListOnly`,
`-Preflight`, `-Item`, and `-Port` parameters.
For each workflow only its newest completed/successful exact-HEAD run is
accepted; an incomplete, expired, empty, missing, or duplicate artifact set
fails closed without an older-run fallback.

Default interactive use lets an operator choose any dynamically derived item,
checks one exact-SHA schema-1 package, probes a selected port, and requires exact
`FLASH` before one non-erasing `write_flash`. The write must report `Hash of data
verified`; the program exits after that one item and never auto-advances. The
profile check is a silicon/configuration check, not evidence of a PCB/electrical
difference. A compile, package, or verified write is not a runtime PASS.

ESP-IDF package acceptance also compares every verified manifest file's original
metadata path with bundled `metadata/flasher_args.json`; missing referenced
images are rejected. Arduino acceptance compares the manifest, command, and
every verified segment exactly with bundled `metadata/flash_args`, preserves
its validated flash geometry flags, and rejects any merged/whole-flash path.

## Factory and C6 firmware

CI-built packages are not factory/recovery images. Vendor factory/recovery
artifacts, if authorized and added later, need their own source/version/hash
record and should not be rebuilt by CI.

The ESP32-C6 coprocessor firmware is a separate image and is not present in any
P4 CI package. Do not include or flash a guessed C6 image with a P4 package. See
`../docs/p4-c6-hosted-wifi.md`.

## Release acceptance

Before publishing a package:

1. Build from a clean source revision with exact framework versions.
2. Compare ESP-IDF files with `flasher_args.json`, or Arduino segments and safe
   write options with `flash_args`.
3. Verify all package hashes and both flash scripts.
4. Flash the complete package on a named board revision.
5. For Arduino, cold-start with the serial monitor disconnected; after normal
   application entry, attach the CH343P UART monitor and confirm no reset/hang.
6. Test the peripherals the package claims to support.
7. Record C6 compatibility when Wi-Fi is included.
8. Review licensing, credentials, host-local paths, and user data.

Compile or package success alone is not hardware validation.
