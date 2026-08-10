# Third-Party Notices

[English](THIRD_PARTY_NOTICES.md) · [简体中文](THIRD_PARTY_NOTICES_ZH.md)

This file is an inventory aid. It does not replace the license text shipped
with a component, resolve conflicting metadata, grant a repository-wide
license, or constitute legal advice. Exact contents of a distribution may vary;
review and update this file for every release.

## Official Waveshare product resources

Normalized examples, the product-specific Arduino display/touch helper,
schematics, and mechanical drawings originated from the official
ESP32-P4-WIFI6-Touch-LCD-4B resources listed in `docs/sources.md`.

No archive-wide top-level license was found in the official example ZIP
reviewed on 2026-07-30. Confirm redistribution terms with the rights holder
before mirroring or publishing material that lacks individual terms.

## Waveshare managed components

| Component | Version/baseline | Notice |
| --- | --- | --- |
| `waveshare/esp32_p4_wifi6_touch_lcd_4b` | 3.0.0 | Upstream package includes an Apache-2.0 license text; it applies to that component only |
| `waveshare/esp_lcd_st7703` | 2.0.0 | License metadata is inconsistent: manifest says MIT, supplied MIT text has unfilled placeholders, and source headers say Apache-2.0; upstream clarification is required |

Managed dependencies may not be stored in the repository, but their licenses
can still apply to source or binary distributions that include them.

## Arduino libraries

| Library | Version baseline | License notice |
| --- | --- | --- |
| GFX Library for Arduino | 1.6.0 in the official archive; 1.6.6 in repository CI | Upstream packages include a BSD-style license file; retain the copyright and conditions for the version actually distributed |
| LVGL | 9.3.0 | Upstream package includes MIT terms; optional libraries, fonts, and generated assets may carry additional notices |

The repository may install these generic libraries during the build instead of
vendoring their large source trees. That choice does not remove their license
obligations.

## Espressif-derived material

Imported ESP-IDF example files and managed Espressif components can carry
individual SPDX headers and license files. Preserve those headers and the
license material belonging to the exact component versions resolved by a
build.

The local Brookesia core includes Apache-2.0 terms at
`firmware/brookesia/components/brookesia_core/license.txt`. Those terms apply
to that component and do not select a license for the whole repository.

## Xiaozhi material

The third-party Xiaozhi directory under
`firmware/brookesia/components/XiaozhiApp/third_party/xiaozhi_esp32/` includes
MIT terms. Preserve that license with source or binary distributions as
required. Historical material under `firmware/brookesia/archive/` must be
reviewed independently if it is included in a release.

## Media, fonts, and artwork

The following categories require file-level provenance and license review:

- `examples/esp-idf/audio-codec/main/canon.pcm`.
- OGG prompts under `firmware/brookesia/spiffs/`.
- Music tracks supplied for the Brookesia music player.
- Application icons and their generated LVGL C arrays.
- Fonts, speech models, and generated model images.
- Media or assets downloaded through managed components.

Do not add or distribute an asset merely because the firmware can consume it.
Record its source, author/rightsholder, exact license, and any attribution or
share-alike requirements.

## Hardware documents

The schematic PDFs and DXF/PDF dimensional drawings are vendor reference
materials. No open-hardware or general redistribution license was established
by the source audit. Their official URLs and hashes prove provenance, not
permission.

## Repository status

No repository-wide `LICENSE.txt` has been selected. See `docs/licensing.md`
before public publication, redistribution, or firmware release.
