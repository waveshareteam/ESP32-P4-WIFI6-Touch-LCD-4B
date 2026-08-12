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
./flash.sh
```

On Windows:

```bat
flash.cmd
```

The generated scripts use the default port discovery behavior of esptool. Add
an explicit port only through a reviewed copy or an equivalent esptool command
when multiple devices are attached.

A Brookesia package must include every image referenced by its generated flash
arguments, including model and SPIFFS images. An application-only binary is
not a complete first-install package.

## Retrieve a CI artifact

The ESP-IDF, Arduino, and Brookesia workflows package after a successful build
and upload one ZIP with `if-no-files-found: error` and 14-day retention. Artifact
names are `firmware-esp-idf-<name>-<idf-version>-rev1_3`,
`firmware-arduino-<name>-3.3.11-rev1_3`, and
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

All example and Arduino packages are `rev1_3`/pre-v3 only; the example matrices
remain 26 ESP-IDF builds and five Arduino builds rather than being doubled.
Brookesia is the only maintained product firmware with two profile-safe
artifacts. `rev1_3` declares a 1.0 minimum and `<3.0` maximum; `rev3_x` declares
a 3.0 minimum and deliberately makes no unverified upper hardware claim.
Never cross-flash the profiles. Before a v3.x flash, confirm the matching
PCB/electrical revision as well as the chip revision.

## Arduino boundary

Arduino packages require the exact 32 MiB pre-v3 FQBN with `FlashSize=32M`,
`ChipVariant=prev3`, and `EraseFlash=none`. The packager accepts one merged
binary at offset zero, or exactly one bootloader, partition table, OTA data, and
application binary at their defined offsets. Any ambiguity is an error.

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
verified`; the program exits after that one item and never auto-advances. A v3.x
chip still requires independent PCB/electrical confirmation. A compile, package,
or verified write is not a runtime PASS.

ESP-IDF package acceptance also compares every verified manifest file's original
metadata path with bundled `metadata/flasher_args.json`; missing referenced
images are rejected. Arduino acceptance requires the exact repository CI FQBN
and a merged-at-zero or complete four-image layout.

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
2. Compare packaged files and offsets with `flasher_args.json`.
3. Verify all package hashes and both flash scripts.
4. Flash the complete package on a named board revision.
5. Test the peripherals the package claims to support.
6. Record C6 compatibility when Wi-Fi is included.
7. Review licensing, credentials, host-local paths, and user data.

Compile or package success alone is not hardware validation.
