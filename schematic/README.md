# Schematics

[English](README.md) · [简体中文](README_ZH.md)

This directory contains references for two distinct hardware assemblies. Do
not use a bottom-board schematic as though it described the standalone display
board.

| File | Scope | Provenance | Status |
| --- | --- | --- | --- |
| `ESP32-P4-WIFI6-Touch-LCD-4B.pdf` | Main ESP32-P4 display board | Current official product download | Primary main-board reference |
| `86_Panel_Bottom_Board.pdf` | 86-panel Ethernet/relay/RS485 bottom board | Current official product download | Primary bottom-board reference |
| `86 Panel Bottom Board V1.3(1).pdf` | Historical 86-panel bottom board V1.3 | Pre-existing repository copy; exact current download URL not verified | Historical revision reference |

Official URLs, file sizes, hashes, and retrieval date are recorded in
`../docs/sources.md`.

## Hardware boundary

The main-board schematic covers the ESP32-P4, display, touch, audio, camera,
MicroSD, USB, and ESP32-C6 connections. The bottom-board schematic covers the
IP101 Ethernet PHY, relays, and RS485 transceiver.

An example using Ethernet, a relay, or RS485 requires the complete matching
86-panel hardware assembly. The standalone 4B board does not contain those
circuits, and the official product page identifies different main-PCB and
enclosure arrangements rather than a drop-in bottom-board upgrade.

## Revision-specific findings

The historical V1.3 bottom-board drawing ties RS485 `/RE` and `DE` and pulls
the net high, which can hold the transceiver in transmit mode and disable its
receiver. That finding applies to the reviewed V1.3 drawing. Confirm the actual
board and the current official bottom-board schematic before applying it to
another revision.

Do not short RS485 A and B for loopback testing, and do not attach another
actively driving adapter until direction control and bus ownership are known.

## Review rules

- Cite the exact file and visible revision when documenting a pin or circuit.
- Cross-check schematic facts against `../docs/board.md`, BSP constants, and
  the example using the peripheral.
- Treat compilation as API validation only; it does not verify a circuit.
- Record unresolved differences instead of selecting the value from the newest
  file by assumption.
- Preserve original vendor filenames where practical so source hashes remain
  easy to verify.

This documentation/import pass did not perform electrical measurements or
on-board validation.
