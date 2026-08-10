# Licensing and Redistribution Boundaries

[English](licensing.md) · [简体中文](licensing_ZH.md)

No repository-wide license has been selected. Do not infer one from a
subcomponent, an SPDX header, a dependency manifest, or an upstream repository.
This document records observed boundaries; it is not legal advice and does not
grant permission to use or redistribute any material.

## Provenance is not permission

`docs/sources.md` records official URLs and SHA-256 hashes. Those records show
where the imported material came from and which archive was reviewed. They do
not establish that every file may be republished, modified, sold, or included
in a binary product.

The official product example archive reviewed on 2026-07-30 did not contain an
archive-wide top-level license. Obtain an explicit repository license or other
authorization from the rights holder before public redistribution of imported
material whose individual terms are absent.

## Repository-wide license

Do not add `LICENSE.txt` by copying a license from any of these locations:

- `firmware/brookesia/components/brookesia_core/license.txt`.
- A Waveshare BSP or driver package.
- GFX Library for Arduino or LVGL.
- Xiaozhi third-party code.
- An ESP-IDF example source header.

Those terms apply only to their respective material. The repository owner must
choose and document a repository-wide license that is compatible with all
included source and assets.

## Imported Waveshare material

The normalized ESP-IDF examples, Arduino sketches, product-specific Arduino
helper, schematics, and 2D drawings originated from the official product
resources. Preserve source records and vendor notices. Until redistribution
rights are confirmed, linking to an official download is not equivalent to
permission to mirror its contents.

Hardware PDFs and DXF files are reference documents, not automatically
open-source hardware design files. Do not claim a right to manufacture,
modify, or redistribute them without the applicable vendor terms.

## Managed components

Managed dependencies remain separate works even when downloaded during a
build or linked into a firmware image. Preserve their package licenses and
notices in any distribution that requires them.

The published Waveshare 4B BSP 3.0.0 package includes an Apache-2.0 license
text. That does not license this repository as a whole.

The ST7703 2.0.0 material needs upstream clarification: its component manifest
identifies MIT, the supplied MIT text contains unfilled copyright placeholders,
and source headers identify Apache-2.0. Do not resolve that conflict by editing
the license locally or asserting one interpretation. Track an upstream
correction and preserve all notices in the meantime.

## Third-party source and libraries

- The official example archive bundles GFX Library for Arduino 1.6.0, while
  repository CI installs 1.6.6. Both upstream packages include a BSD-style
  license file; release notices must match the version actually distributed.
- LVGL 9.3.0 includes an MIT license file and additional licenses for bundled
  fonts and optional libraries.
- The local Brookesia core includes Apache-2.0 terms that apply to that
  component.
- The Xiaozhi third-party directory includes MIT terms that apply to that
  directory.
- Espressif-derived example files may carry individual SPDX identifiers; keep
  those headers intact.

If generic Arduino libraries are installed by the build environment instead of
vendored, release and source distributions still need to satisfy the licenses
of the versions actually used.

## Media and generated assets

Treat audio, fonts, icons, generated C arrays, and model data separately from
source code. In particular:

- Confirm provenance and redistribution terms for `canon.pcm`.
- Confirm the OGG prompts under `firmware/brookesia/spiffs/`.
- Do not add MP3 tracks until their redistribution rights are recorded.
- Retain font licenses and notices when font binaries or generated arrays are
  redistributed.
- Record the source and license for application icons and other artwork.

A file being usable at runtime does not establish permission to distribute it.

## Before publication or release

1. Select the repository-wide license through the project owner.
2. Resolve or remove files with unknown or conflicting terms.
3. Preserve all per-file SPDX headers and bundled license files.
4. Update `../THIRD_PARTY_NOTICES.md` with the exact versions and materials
   included in the distribution.
5. Review generated firmware for linked-component notice obligations.
6. Review hardware documents and media separately from source code.
7. Confirm that no local paths, credentials, or user data appear in public
   text or artifacts.

Until those steps are complete, the absence of a root license should be read
as “no repository-wide permission has been granted,” not as a choice of a
default license.
