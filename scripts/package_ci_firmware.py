#!/usr/bin/env python3
"""Package traceable ESP32-P4 CI firmware without embedding unsafe flash actions."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath

BOARD = "ESP32-P4-WIFI6-Touch-LCD-4B"
CHIP = "esp32p4"
FLASH_SIZE = 32 * 1024 * 1024
DEFAULT_BAUD = 460800
ARDUINO_FQBN = "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=prev3"
ARDUINO_FQBN_OPTIONS = {"FlashSize": "32M", "ChipVariant": "prev3", "EraseFlash": "none"}
BOARD_PROFILES = {
    "rev1_3": {
        "minimum": "1.0",
        "maximum_exclusive": "3.0",
        "symbols": {"CONFIG_ESP32P4_SELECTS_REV_LESS_V3": "y", "CONFIG_ESP32P4_REV_MIN_100": "y"},
    },
    "rev3_x": {
        "minimum": "3.0",
        "maximum_exclusive": None,
        "symbols": {"CONFIG_ESP32P4_SELECTS_REV_LESS_V3": "n", "CONFIG_ESP32P4_REV_MIN_300": "y"},
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def contained(root: Path, candidate: Path, description: str) -> Path:
    root, candidate = root.resolve(), candidate.resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ValueError(f"{description} escapes its declared directory") from exc
    if not candidate.is_file():
        raise FileNotFoundError(f"{description} is missing: {candidate}")
    return candidate


def offset(value: object) -> int:
    try:
        parsed = int(str(value), 0)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"Invalid flash offset: {value!r}") from exc
    if parsed < 0:
        raise ValueError(f"Negative flash offset: {value!r}")
    return parsed


def git_sha() -> str:
    value = os.environ.get("PACKAGE_GIT_SHA") or os.environ.get("GITHUB_SHA", "")
    if os.environ.get("CI", "").lower() == "true" and not re.fullmatch(r"[0-9a-fA-F]{40}", value):
        raise ValueError("CI packaging requires a complete 40-character PACKAGE_GIT_SHA")
    if value and not re.fullmatch(r"[0-9a-fA-F]{40}", value):
        raise ValueError("PACKAGE_GIT_SHA must be a complete 40-character SHA")
    return value.lower() or "unknown"


def archive_name(source: Path, used: set[str]) -> str:
    candidate = f"bin/{source.name}"
    stem, suffix, index = source.stem, source.suffix, 2
    while candidate in used:
        candidate = f"bin/{stem}-{index}{suffix}"
        index += 1
    used.add(candidate)
    return candidate


def repository_relative(project: Path) -> str:
    try:
        return project.resolve().relative_to(Path.cwd().resolve()).as_posix()
    except ValueError as exc:
        raise ValueError("Project must be contained by the repository working directory") from exc


def parse_arduino_fqbn(fqbn: str) -> dict[str, str]:
    if fqbn != ARDUINO_FQBN:
        raise ValueError("Arduino FQBN must exactly match the repository CI contract")
    parts = fqbn.split(":", 3)
    if len(parts) != 4 or parts[:3] != ["esp32", "esp32", "esp32p4"]:
        raise ValueError("Arduino FQBN must select esp32:esp32:esp32p4")
    options: dict[str, str] = {}
    for raw_option in parts[3].split(","):
        key, separator, value = raw_option.partition("=")
        if not separator or not key or not value or key in options:
            raise ValueError("Arduino FQBN options must be non-empty and unique key=value pairs")
        options[key] = value
    for key, expected in ARDUINO_FQBN_OPTIONS.items():
        if options.get(key) != expected:
            raise ValueError(f"Arduino FQBN must set {key}={expected} exactly once")
    return options


def board_profile(value: str) -> str:
    if value not in BOARD_PROFILES:
        raise ValueError(f"Unsupported board profile: {value}")
    return value


def sdkconfig_values(build: Path) -> dict[str, str]:
    """Read the generated IDF configuration from its documented build locations."""
    json_path = build / "config" / "sdkconfig.json"
    if json_path.is_file():
        raw = json.loads(json_path.read_text(encoding="utf-8"))
        if isinstance(raw, dict):
            return {
                str(key) if str(key).startswith("CONFIG_") else f"CONFIG_{key}": "y" if value is True else "n" if value is False else str(value)
                for key, value in raw.items()
            }
    for candidate in (build / "sdkconfig", build / "config" / "sdkconfig"):
        if candidate.is_file():
            values: dict[str, str] = {}
            for line in candidate.read_text(encoding="utf-8").splitlines():
                if line.startswith("# CONFIG_") and line.endswith(" is not set"):
                    values[line[2:-11]] = "n"
                elif line.startswith("CONFIG_") and "=" in line:
                    key, value = line.split("=", 1)
                    values[key] = value
            return values
    raise ValueError("ESP-IDF build configuration is missing (expected config/sdkconfig.json or sdkconfig)")


def validate_idf_profile(build: Path, profile: str) -> None:
    values = sdkconfig_values(build)
    for symbol, expected in BOARD_PROFILES[profile]["symbols"].items():
        if values.get(symbol) != expected:
            raise ValueError(f"ESP-IDF build configuration does not match {profile}: {symbol}={expected}")


def validate_idf_write_flash_args(raw: object) -> list[str]:
    if raw is None:
        return []
    if not isinstance(raw, list) or not all(isinstance(item, str) and item for item in raw):
        raise ValueError("ESP-IDF write_flash_args must be an array of non-empty strings")
    dangerous = ("erase", "read_flash", "write_reg", "run", "execute", "reset")
    if any(any(token in item.casefold() for token in dangerous) for item in raw):
        raise ValueError("ESP-IDF write_flash_args contains an unsupported dangerous operation")
    return list(raw)


def validate_plan(records: list[dict[str, object]]) -> None:
    if not records:
        raise ValueError("No flashable binaries were found")
    ordered = sorted(records, key=lambda item: offset(item["offset"]))
    previous_end = -1
    seen: set[int] = set()
    for record in ordered:
        start, size = offset(record["offset"]), int(record["size"])
        if size <= 0 or start in seen or start < previous_end or start + size > FLASH_SIZE:
            raise ValueError("Flash ranges must be unique, non-overlapping, and within 32 MiB")
        seen.add(start)
        previous_end = start + size


def flash_helpers(records: list[dict[str, object]]) -> tuple[str, str, str]:
    pairs = " ".join(f"{item['offset']} {item['archive_path']}" for item in sorted(records, key=lambda x: offset(x['offset'])))
    command = f"python -m esptool --chip {CHIP} --baud {DEFAULT_BAUD} write_flash {pairs}"
    shell = "#!/usr/bin/env sh\nset -eu\nDIR=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\n" + f"python -m esptool \"$@\" --chip {CHIP} --baud {DEFAULT_BAUD} write_flash " + " ".join(f"{item['offset']} \"$DIR/{item['archive_path']}\"" for item in sorted(records, key=lambda x: offset(x['offset']))) + "\n"
    batch = "@echo off\r\n" + f"python -m esptool %* --chip {CHIP} --baud {DEFAULT_BAUD} write_flash " + " ".join(f"{item['offset']} \"%~dp0{str(item['archive_path']).replace('/', '\\\\')}\"" for item in sorted(records, key=lambda x: offset(x['offset']))) + "\r\nif errorlevel 1 exit /b %errorlevel%\r\n"
    return command, shell, batch


def manifest(framework: str, version: str, project: Path, records: list[dict[str, object]], command: str, profile: str, fqbn: str | None = None) -> dict[str, object]:
    contract = BOARD_PROFILES[profile]
    document: dict[str, object] = {"schema_version": 1, "board": BOARD, "chip": CHIP, "board_profile": profile, "chip_revision": {"minimum": contract["minimum"], "maximum_exclusive": contract["maximum_exclusive"], "maximum_note": "No validated hardware upper bound is claimed." if contract["maximum_exclusive"] is None else None}, "c6_firmware_included": False, "framework": framework, "framework_version": version, "source_project": repository_relative(project), "git_sha": git_sha(), "generated_at_utc": datetime.now(timezone.utc).isoformat(), "flash": {"baud": DEFAULT_BAUD, "size_bytes": FLASH_SIZE, "command": command}, "files": records}
    if fqbn:
        document["fqbn"] = fqbn
    return document


def write_bundle(output: Path, records: list[dict[str, object]], sources: list[Path], document: dict[str, object], extras: list[tuple[Path, str]] = []) -> Path:
    if output.exists():
        raise FileExistsError(f"Refusing to overwrite package: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    _, shell, batch = flash_helpers(records)
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for source, record in zip(sources, records): archive.write(source, str(record["archive_path"]))
        for source, name in extras: archive.write(source, name)
        archive.writestr("manifest.json", json.dumps(document, indent=2, sort_keys=True) + "\n")
        archive.writestr("flash.sh", shell)
        archive.writestr("flash.cmd", batch)
    return output


def package_idf(project: Path, build: Path, version: str, output: Path, profile: str) -> Path:
    project, build = project.resolve(), build.resolve()
    validate_idf_profile(build, board_profile(profile))
    args_path = contained(build, build / "flasher_args.json", "ESP-IDF flasher metadata")
    data = json.loads(args_path.read_text(encoding="utf-8"))
    flash_files = data.get("flash_files")
    if not isinstance(flash_files, dict) or not flash_files: raise ValueError("flasher_args.json has no flash_files map")
    write_flash_args = validate_idf_write_flash_args(data.get("write_flash_args"))
    used: set[str] = set(); records: list[dict[str, object]] = []; sources: list[Path] = []
    for raw_offset, raw_path in sorted(flash_files.items(), key=lambda item: offset(item[0])):
        if not isinstance(raw_path, str) or not raw_path: raise ValueError("Invalid ESP-IDF flash file path")
        if "esp32c6" in raw_path.casefold(): raise ValueError("P4 CI packages cannot include an ESP32-C6 flash image")
        relative = PurePosixPath(raw_path.replace("\\", "/"))
        if relative.is_absolute() or ".." in relative.parts: raise ValueError("Unsafe ESP-IDF flash file path")
        source = contained(build, build.joinpath(*relative.parts), "ESP-IDF flash binary")
        records.append({"offset": f"0x{offset(raw_offset):x}", "archive_path": archive_name(source, used), "metadata_path": relative.as_posix(), "size": source.stat().st_size, "sha256": sha256(source)})
        sources.append(source)
    validate_plan(records); command, _, _ = flash_helpers(records)
    document = manifest("esp-idf", version, project, records, command, profile)
    document["idf_write_flash_args"] = write_flash_args
    return write_bundle(output, records, sources, document, [(args_path, "metadata/flasher_args.json")])


def package_arduino(project: Path, build: Path, version: str, output: Path, fqbn: str, profile: str) -> Path:
    if board_profile(profile) != "rev1_3":
        raise ValueError("Arduino ChipVariant=prev3 packages must use board profile rev1_3")
    parse_arduino_fqbn(fqbn)
    project, build = project.resolve(), build.resolve()
    binaries = [contained(build, path, "Arduino binary") for path in sorted(build.rglob("*.bin"))]
    merged = [path for path in binaries if "merged" in path.name.lower()]
    if len(merged) > 1: raise ValueError("Arduino output has ambiguous merged binaries")
    selected: list[tuple[Path, str]]
    if merged: selected = [(merged[0], "0x0")]
    else:
        def exactly(marker: str) -> Path:
            matches = [path for path in binaries if marker in path.name.lower()]
            if len(matches) != 1: raise ValueError(f"Expected exactly one Arduino {marker} binary")
            return matches[0]
        boot, parts, ota = exactly("bootloader"), exactly("partitions"), exactly("boot_app0")
        app = [path for path in binaries if path not in {boot, parts, ota}]
        expected = [path for path in app if path.name in {project.name + ".bin", project.name + ".ino.bin"}]
        if len(expected) != 1: raise ValueError("Expected exactly one Arduino application binary")
        selected = [(boot, "0x0"), (parts, "0x8000"), (ota, "0xe000"), (expected[0], "0x10000")]
    used: set[str] = set(); records: list[dict[str, object]] = []
    for source, raw_offset in selected:
        records.append({"offset": raw_offset, "archive_path": archive_name(source, used), "size": source.stat().st_size, "sha256": sha256(source)})
    validate_plan(records); command, _, _ = flash_helpers(records)
    return write_bundle(output, records, [source for source, _ in selected], manifest("arduino-esp32", version, project, records, command, profile, fqbn))


def main() -> int:
    parser = argparse.ArgumentParser(); subs = parser.add_subparsers(dest="kind", required=True)
    for kind in ("esp-idf", "arduino"):
        sub = subs.add_parser(kind); sub.add_argument("--project", type=Path, required=True); sub.add_argument("--build-dir", type=Path, required=True); sub.add_argument("--framework-version", required=True); sub.add_argument("--board-profile", choices=tuple(BOARD_PROFILES), required=True); sub.add_argument("--output", type=Path, required=True)
        if kind == "arduino": sub.add_argument("--fqbn", required=True)
    args = parser.parse_args()
    output = package_idf(args.project, args.build_dir, args.framework_version, args.output, args.board_profile) if args.kind == "esp-idf" else package_arduino(args.project, args.build_dir, args.framework_version, args.output, args.fqbn, args.board_profile)
    print(output.as_posix()); return 0


if __name__ == "__main__": raise SystemExit(main())
