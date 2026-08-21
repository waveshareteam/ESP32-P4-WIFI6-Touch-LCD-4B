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
| `waveshare/esp32_p4_wifi6_touch_lcd_4b` | 3.0.1 | Upstream package includes an Apache-2.0 license text; it applies to that component only |
| `waveshare/esp_lcd_st7703` | 2.0.0 | License metadata is inconsistent: manifest says MIT, supplied MIT text has unfilled placeholders, and source headers say Apache-2.0; upstream clarification is required |

Managed dependencies may not be stored in the repository, but their licenses
can still apply to source or binary distributions that include them.

## Arduino libraries

| Library | Version baseline | License notice |
| --- | --- | --- |
| GFX Library for Arduino | 1.6.0, complete bundled copy used by repository CI | The bundled directory includes the upstream BSD-style license file; retain its copyright and conditions |
| LVGL | 9.3.0 | Upstream package includes MIT terms; optional libraries, fonts, and generated assets may carry additional notices |

Repository Arduino CI consumes these complete bundled libraries through its
`--libraries` path. It does not replace them with separately installed
Registry versions.

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

## Checked-in Brookesia rev3_x image

The default source-built image
`firmware/ESP32-P4-WIFI6-Touch-LCD-4B-Brookesia-rev3_x-260821.bin` was built
from repository commit `f417f6b764f06dddb89fd4f30730ecf4b1fc56d3` and embeds
the application, speech model, and SPIFFS storage payload. In addition to the
Waveshare BSP and local source notices above, its release inventory includes:

| Material in image | Version/source | Recorded upstream terms |
| --- | --- | --- |
| Xiaozhi activation, digit, and success OGG prompts | `78/xiaozhi-esp32` commit `0ec696f64f5843ca0f5fcf700ae45977d1dcd2e8` | The matching local adaptation directory includes an MIT license that explicitly covers the imported recordings |
| Xiaozhi PuHui font storage payload | [`78/xiaozhi-fonts` 1.6.0](https://components.espressif.com/components/78/xiaozhi-fonts/versions/1.6.0/readme) | The ESP Component Registry declares MIT; the resolved package did not supply a separate license/copyright file in this repository, so this record does not grant additional rights or replace upstream terms |
| ESP-SR `wn9_nihaoxiaozhi_tts` speech model | [`espressif/esp-sr` 2.4.7](https://components.espressif.com/components/espressif/esp-sr/versions/2.4.7/license?language=en) | ESPRESSIF MIT; retain the upstream copyright and permission notice, and observe its Espressif-product condition |
| Brookesia GUI runtime | [`lvgl/lvgl` 9.4.0](https://components.espressif.com/components/lvgl/lvgl/versions/9.4.0/license?language=en) | Upstream MIT; the enabled built-in Montserrat and Source Han Sans SC fonts carry their upstream SIL OFL-1.1 notices |
| Local Brookesia core and generated Maison Neue arrays | source tree at the image source commit | The component distributes these files with its Apache-2.0 terms; no separate license for the original Maison Neue font file was found, and this notice makes no additional claim about that original file |
| Waveshare product application code and UI assets | source tree at the image source commit | Official Waveshare product resources; no archive-wide downstream license is inferred or granted by this notice |

This is the exact resource notice for the checked-in image. It is not a
repository-wide license, does not resolve the separately documented ST7703
metadata conflict, and does not cover a later rebuilt image unless its resolved
components and payload are reviewed again.

Publication of this exact dated image was explicitly authorized for this
repository on 2026-08-21 without selecting a repository-wide license. That
authorization grants no repository-wide or additional asset permission and
does not replace applicable third-party licenses or notices.

## Media, fonts, and artwork

The following categories still require file-level provenance and license review
when they are included in a distribution not covered by the dated image notice
above:

- `examples/esp-idf/audio-codec/main/canon.pcm`.
- Music tracks supplied for the Brookesia music player.
- Application icons and their generated LVGL C arrays.
- Fonts, speech models, and generated model images other than the exact versions
  recorded for the checked-in image.
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
