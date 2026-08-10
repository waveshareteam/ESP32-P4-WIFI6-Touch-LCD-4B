# Hardware References

[English](README.md) · [简体中文](README_ZH.md)

`hardware/dimensions/` contains the mechanical files extracted from the
official ESP32-P4-WIFI6-Touch-LCD-4B 2D resource archive retrieved on
2026-07-30.

| File | Format | Purpose |
| --- | --- | --- |
| `dimensions/ESP32-P4-WIFI6-Touch-LCD-4B.dxf` | DXF | CAD/mechanical reference |
| `dimensions/ESP32-P4-WIFI6-Touch-LCD-4B.pdf` | PDF | Human-readable 2D drawing |

The original archive URL, archive hash, and extracted-file hashes are recorded
in `../docs/sources.md`.

## Use boundary

These files describe dimensions; they are not substitutes for electrical
schematics, PCB source, manufacturing data, or a verified enclosure model. Use
`../schematic/` for circuit review and `../docs/board.md` for the maintained
pin map.

Before manufacturing an enclosure or panel:

- Confirm the board and drawing revision.
- Confirm units and scale in the original CAD application.
- Measure critical connector, mounting-hole, display-active-area, and cable
  clearances on a physical sample.
- Account for the separate 86-panel product assembly when working on that variant.
- Do not infer tolerances that are not stated in the source drawing.

No dimensional or fit check was performed during this documentation/import
pass.

## Licensing

Official-source provenance does not itself grant redistribution or
manufacturing rights. Review `../docs/licensing.md` before publishing or
redistributing the DXF or PDF.
