# Firmware Packages

This directory documents generated firmware packages. Generated archives are
not committed; the default output locations `release-artifacts/`,
`releases/dist/`, and `releases/downloads/` are ignored.

No release-package generation, flash, or hardware test is recorded for this
repository state. Existing ignored local build directories are not release
evidence.

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

The current workflows perform compile validation only and do not upload
firmware packages automatically.

When the workflow uploads a package, it can be downloaded with the GitHub web
interface or GitHub CLI:

```sh
gh run download RUN_ID --name ARTIFACT_NAME --dir releases/downloads
```

Record the workflow run, source commit, framework version, and artifact SHA-256
when promoting a CI artifact to a release.

## Arduino boundary

Arduino sketches are currently compile-validation targets. Do not label an
Arduino ZIP as flashable unless it contains the correct bootloader, partition
table, application, and offsets for the exact FQBN and board options.

## Factory and C6 firmware

CI-built packages are not factory/recovery images. Vendor factory/recovery
artifacts, if authorized and added later, need their own source/version/hash
record and should not be rebuilt by CI.

The ESP32-C6 coprocessor firmware is a separate image and is not present in the
official example archive reviewed for this repository. Do not include or flash
a guessed C6 image with a P4 package. See `../docs/p4-c6-hosted-wifi.md`.

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
