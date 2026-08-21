#!/usr/bin/env python3
"""Package traceable ESP32-P4 CI firmware without embedding unsafe flash actions."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath

BOARD = "ESP32-P4-WIFI6-Touch-LCD-4B"
CHIP = "esp32p4"
FLASH_SIZE = 32 * 1024 * 1024
DEFAULT_BAUD = 460800
ARDUINO_CORE_VERSION = "3.3.11"
ARDUINO_FQBN = "esp32:esp32:esp32p4:UploadSpeed=921600,USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,FlashFreq=80,FlashMode=qio,FlashSize=32M,PartitionScheme=app13M_data7M_32MB,DebugLevel=none,PSRAM=enabled,EraseFlash=none,JTAGAdapter=default,ChipVariant=postv3"
ARDUINO_FQBN_OPTIONS = {"FlashSize": "32M", "ChipVariant": "postv3", "EraseFlash": "none"}
ARDUINO_BSP_COMPONENT = "waveshare/esp32_p4_wifi6_touch_lcd_4b"
ARDUINO_FLASH_FLAGS = ("--flash-mode", "--flash-freq", "--flash-size")
REPO_ROOT = Path(__file__).resolve().parents[1]
PRIVATE_ARTIFACT_PATTERNS = (
    re.compile(rb"/home/[^/\x00\s\"'<>]+(?=[/\\\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    re.compile(rb"/Users/[^/\x00\s\"'<>]+(?=[/\\\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    re.compile(rb"/(?:private/)?tmp(?=[/\\\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    re.compile(rb"/root(?=[/\\\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    re.compile(rb"/workspaces?(?=[/\\\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    # Any drive-rooted path and any complete UNC server/share root exposes the
    # producer's host layout.  Relative "C:filename" text remains legal.
    re.compile(rb"(?<![A-Za-z0-9])[A-Z]:[\\/]", re.IGNORECASE),
    re.compile(
        rb"\\\\[A-Za-z0-9._-]+[\\/][A-Za-z0-9$._-]+(?=[\\/\x00\s\"'<>]|$)",
        re.IGNORECASE,
    ),
    re.compile(
        rb"//[A-Za-z0-9._-]+/[A-Za-z0-9$._-]+(?=[/\x00\s\"'<>]|$)",
        re.IGNORECASE,
    ),
    re.compile(rb"\.arduino15(?=[\\/\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    re.compile(rb"/var/folders(?=[/\\\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    re.compile(rb"[\\/]AppData[\\/](?:Local|Roaming)[\\/]", re.IGNORECASE),
    re.compile(rb"/opt/hostedtoolcache(?=[/\\\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    re.compile(rb"/(?:__w|_work)(?=[/\\\x00\s\"'<>),;\]}]|$)", re.IGNORECASE),
    re.compile(rb"[A-Z]:[\\/]+a[\\/]", re.IGNORECASE),
    re.compile(rb"[A-Z]:[\\/]+hostedtoolcache[\\/]", re.IGNORECASE),
)
BOARD_PROFILES = {
    "rev1_3": {
        "minimum": "1.0",
        "maximum_exclusive": "2.0",
        "maximum_note": "ESP32-P4 rev1.x silicon profile: 1.00 through 1.99.",
        "symbols": {"CONFIG_ESP32P4_SELECTS_REV_LESS_V3": "y", "CONFIG_ESP32P4_REV_MIN_100": "y"},
    },
    "rev3_x": {
        "minimum": "3.0",
        "maximum_exclusive": "4.0",
        "maximum_note": "ESP32-P4 rev3.x silicon profile: 3.00 through 3.99.",
        "symbols": {"CONFIG_ESP32P4_SELECTS_REV_LESS_V3": "n", "CONFIG_ESP32P4_REV_MIN_300": "y"},
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def reject_private_artifact_bytes(payload: bytes, description: str) -> None:
    if any(pattern.search(payload) for pattern in PRIVATE_ARTIFACT_PATTERNS):
        raise ValueError(f"{description} contains a host-specific path")


def contained(root: Path, candidate: Path, description: str) -> Path:
    root, candidate = root.resolve(), candidate.resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ValueError(f"{description} escapes its declared directory") from exc
    if not candidate.is_file():
        raise FileNotFoundError(f"{description} is missing: {candidate}")
    return candidate


def contained_without_symlinks(root: Path, candidate: Path, description: str) -> Path:
    """Resolve a regular file while rejecting every symlink in its relative path."""
    root = root.resolve()
    unresolved = candidate if candidate.is_absolute() else root / candidate
    try:
        relative = unresolved.relative_to(root)
    except ValueError as exc:
        raise ValueError(f"{description} escapes its declared directory") from exc
    cursor = root
    for part in relative.parts:
        cursor /= part
        if cursor.is_symlink():
            raise ValueError(f"{description} uses a symlink: {relative.as_posix()}")
    return contained(root, unresolved, description)


def offset(value: object) -> int:
    try:
        parsed = int(str(value), 0)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"Invalid flash offset: {value!r}") from exc
    if parsed < 0:
        raise ValueError(f"Negative flash offset: {value!r}")
    return parsed


def _repository_root_from_script() -> Path:
    """Return the sole production trust anchor; tests may mock this private seam."""
    return REPO_ROOT


def _git_output(root: Path, arguments: list[str], description: str) -> str:
    try:
        completed = subprocess.run(
            ["git", "-C", str(root), *arguments],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        detail = getattr(exc, "stderr", "") or getattr(exc, "stdout", "") or str(exc)
        raise ValueError(f"Unable to verify trusted repository {description}: {detail.strip()}") from exc
    return completed.stdout.strip()


def _git_blob(root: Path, object_name: str, description: str) -> bytes:
    """Return the exact binary content of a Git object at the trusted HEAD."""
    try:
        completed = subprocess.run(
            ["git", "-C", str(root), "show", object_name],
            check=True,
            capture_output=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        detail = getattr(exc, "stderr", b"") or getattr(exc, "stdout", b"") or str(exc).encode("utf-8")
        raise ValueError(
            f"Unable to verify trusted repository {description}: "
            f"{detail.decode('utf-8', errors='replace').strip()}"
        ) from exc
    return completed.stdout


def trusted_repository() -> tuple[Path, str]:
    """Verify the script-owned Git top level, clean worktree, and exact HEAD."""
    root = _repository_root_from_script().resolve()
    if not root.is_dir():
        raise ValueError("Trusted repository root derived from the packaging script is missing")
    top_level = Path(_git_output(root, ["rev-parse", "--show-toplevel"], "top level")).resolve()
    if top_level != root:
        raise ValueError("Packaging script is not located at the trusted Git top level")
    head = _git_output(root, ["rev-parse", "--verify", "HEAD^{commit}"], "HEAD").lower()
    if not re.fullmatch(r"[0-9a-f]{40}", head):
        raise ValueError("Trusted repository HEAD is not a complete 40-character SHA")
    if _git_output(
        root,
        ["status", "--porcelain=v1", "--untracked-files=all", "--ignore-submodules=none"],
        "worktree status",
    ):
        raise ValueError("Packaging requires a clean trusted repository worktree")
    return root, head


def trusted_package_context() -> tuple[Path, str]:
    root, head = trusted_repository()
    value = os.environ.get("PACKAGE_GIT_SHA", "")
    if not re.fullmatch(r"[0-9a-f]{40}", value) or value != head:
        raise ValueError("PACKAGE_GIT_SHA must exactly equal the clean trusted repository HEAD")
    return root, head


def git_sha() -> str:
    return trusted_package_context()[1]


def primary_sketch_identity(root: Path, source_project: str, head: str) -> dict[str, object]:
    """Resolve the tracked primary sketch in the trusted tree and bind its identity.

    ``source_project`` is the repository-relative sketch directory (for example
    ``examples/arduino/HelloWorld``).  This helper requires that directory and its
    ``<name>.ino`` to exist without symlinks, to be tracked by Git, and to match the
    clean ``head`` tree byte-for-byte.  It returns the canonical primary-source
    identity that the package manifest and validator must agree on.
    """
    if (not _portable_helper_archive_path(source_project)
            or not re.fullmatch(r"[A-Za-z0-9._-]+", PurePosixPath(source_project).name)):
        raise ValueError("Arduino source project is unsafe or unnamed")
    project = PurePosixPath(source_project)
    unresolved_dir = root.joinpath(*project.parts)
    try:
        dir_relative = unresolved_dir.relative_to(root)
    except ValueError as exc:
        raise ValueError("Arduino sketch directory escapes the trusted repository") from exc
    cursor = root
    for part in dir_relative.parts:
        cursor /= part
        if cursor.is_symlink():
            raise ValueError("Arduino sketch directory path uses a symlink")
    project_dir = unresolved_dir.resolve()
    if not project_dir.is_dir():
        raise ValueError("Arduino sketch directory is missing from the trusted repository")
    name = project.name
    sketch_file = contained_without_symlinks(
        root, project_dir / f"{name}.ino", "Arduino primary sketch source"
    )
    relative = sketch_file.resolve().relative_to(root.resolve()).as_posix()
    if _git_output(root, ["ls-files", "--error-unmatch", "--", relative], "tracked primary sketch") != relative:
        raise ValueError("Arduino primary sketch is not uniquely tracked at the trusted HEAD")
    try:
        head_bytes = _git_blob(root, f"{head}:{relative}", "primary sketch HEAD blob")
    except ValueError as exc:
        raise ValueError("Arduino primary sketch is missing from the trusted HEAD tree") from exc
    working_bytes = sketch_file.read_bytes()
    if working_bytes != head_bytes:
        raise ValueError("Arduino primary sketch differs from the clean trusted HEAD")
    blob_sha = _git_output(root, ["rev-parse", "--verify", f"{head}:{relative}"], "primary sketch blob")
    if not re.fullmatch(r"[0-9a-f]{40}", blob_sha):
        raise ValueError("Arduino primary sketch HEAD blob is not a complete 40-character SHA")
    return {
        "path": relative,
        "size": len(head_bytes),
        "sha256": sha256_bytes(head_bytes),
        "git_blob_sha": blob_sha,
    }


def archive_name(source: Path, used: set[str]) -> str:
    candidate = f"bin/{source.name}"
    stem, suffix, index = source.stem, source.suffix, 2
    while candidate in used:
        candidate = f"bin/{stem}-{index}{suffix}"
        index += 1
    used.add(candidate)
    return candidate


def repository_relative(project: Path, root: Path | None = None) -> str:
    trusted_root = (root or trusted_repository()[0]).resolve()
    try:
        return project.resolve().relative_to(trusted_root).as_posix()
    except ValueError as exc:
        raise ValueError("Project must be contained by the trusted repository root") from exc


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


def validate_bsp_binding(version: str, source_git_sha: str, component_git_tree_sha: str) -> tuple[str, str, str]:
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.-]+)?", version):
        raise ValueError("Arduino package BSP version must be an exact semantic version")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", source_git_sha):
        raise ValueError("Arduino package BSP source Git SHA must contain exactly 40 hexadecimal characters")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", component_git_tree_sha):
        raise ValueError("Arduino package BSP component Git tree SHA must contain exactly 40 hexadecimal characters")
    return version, source_git_sha.lower(), component_git_tree_sha.lower()


def flash_size_bytes(value: str) -> int:
    match = re.fullmatch(r"([1-9][0-9]*)\s*(M|MB|MiB)", value, re.IGNORECASE)
    if not match:
        raise ValueError(f"Unsupported Arduino flash size: {value!r}")
    return int(match.group(1)) * 1024 * 1024


def validate_arduino_write_flash_args(raw: object) -> list[str]:
    """Accept only the harmless flash geometry flags emitted by Arduino core."""
    if not isinstance(raw, list) or not raw or not all(isinstance(item, str) and item for item in raw):
        raise ValueError("Arduino flash_args options must be a non-empty string array")
    if len(raw) != len(ARDUINO_FLASH_FLAGS) * 2:
        raise ValueError("Arduino flash_args must contain exactly mode, frequency, and size options")
    values: dict[str, str] = {}
    for index in range(0, len(raw), 2):
        flag, value = raw[index], raw[index + 1]
        if flag not in ARDUINO_FLASH_FLAGS or flag in values:
            raise ValueError("Arduino flash_args contains an unsupported or duplicate option")
        values[flag] = value
    if tuple(values) != ARDUINO_FLASH_FLAGS:
        raise ValueError("Arduino flash_args options are missing or not in the core-generated order")
    if values["--flash-mode"].casefold() not in {"qio", "qout", "dio", "dout"}:
        raise ValueError("Arduino flash_args contains an unsupported flash mode")
    if not re.fullmatch(r"[1-9][0-9]*[mk]", values["--flash-freq"], re.IGNORECASE):
        raise ValueError("Arduino flash_args contains an unsupported flash frequency")
    if flash_size_bytes(values["--flash-size"]) != FLASH_SIZE:
        raise ValueError("Arduino flash_args flash size differs from the 32 MiB FQBN contract")
    return list(raw)


def _arduino_whole_flash_name(value: str) -> bool:
    normalized = value.casefold().replace("_", "-")
    return "merged" in normalized or "whole-flash" in normalized or "wholeflash" in normalized


def is_binary_payload(name: str) -> bool:
    """True for opaque compiled images whose bytes are exempt from text scanning."""
    return PurePosixPath(name).suffix.casefold() == ".bin"


def parse_arduino_flash_args_text(text: str) -> tuple[list[str], list[tuple[int, str]]]:
    """Parse the exact, line-oriented flash_args format produced by Arduino core 3.3.11."""
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if len(lines) < 2:
        raise ValueError("Arduino flash_args is missing options or flash segments")
    try:
        write_flash_args = validate_arduino_write_flash_args(shlex.split(lines[0], posix=True))
    except ValueError:
        raise
    except Exception as exc:
        raise ValueError("Arduino flash_args option line is malformed") from exc
    entries: list[tuple[int, str]] = []
    for line in lines[1:]:
        try:
            tokens = shlex.split(line, posix=True)
        except ValueError as exc:
            raise ValueError("Arduino flash_args segment line is malformed") from exc
        if len(tokens) != 2:
            raise ValueError("Arduino flash_args segment lines must contain exactly offset and file")
        raw_offset, raw_path = tokens
        if "\\" in raw_path or _arduino_whole_flash_name(raw_path):
            raise ValueError("Arduino flash_args references an unsafe or whole-flash image")
        relative = PurePosixPath(raw_path)
        if not _safe_archive_path(raw_path):
            raise ValueError("Arduino flash_args contains an unsafe binary path")
        entries.append((offset(raw_offset), relative.as_posix()))
    if not entries:
        raise ValueError("Arduino flash_args has no flash segments")
    return write_flash_args, entries


def arduino_segment_role(metadata_path: str, project_name: str) -> str:
    name = PurePosixPath(metadata_path).name.casefold()
    if _arduino_whole_flash_name(name):
        raise ValueError("Arduino whole-flash images cannot be release segments")
    if "bootloader" in name:
        return "bootloader"
    if name == "boot_app0.bin":
        return "boot_app0"
    if "partition" in name:
        return "partition_table"
    expected = {f"{project_name.casefold()}.bin", f"{project_name.casefold()}.ino.bin"}
    if name in expected:
        return "application"
    return "toolchain"


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


def validate_plan(records: list[dict[str, object]], flash_capacity: int = FLASH_SIZE) -> None:
    if not records:
        raise ValueError("No flashable binaries were found")
    ordered = sorted(records, key=lambda item: offset(item["offset"]))
    previous_end = -1
    seen: set[int] = set()
    for record in ordered:
        start, size = offset(record["offset"]), int(record["size"])
        if size <= 0 or start in seen or start < previous_end or start + size > flash_capacity:
            raise ValueError("Flash ranges must be unique, non-overlapping, and within flash capacity")
        seen.add(start)
        previous_end = start + size


def flash_helpers(records: list[dict[str, object]], write_flash_args: list[str] | None = None) -> tuple[str, str, str]:
    options = list(write_flash_args or [])
    pairs = " ".join(f"{item['offset']} {item['archive_path']}" for item in sorted(records, key=lambda x: offset(x['offset'])))
    option_text = (" ".join(options) + " ") if options else ""
    command = f"python -m esptool --chip {CHIP} --baud {DEFAULT_BAUD} write_flash {option_text}{pairs}"
    shell = (
        "#!/usr/bin/env sh\n"
        "set -eu\n"
        "if [ \"$#\" -ne 2 ] || [ \"$1\" != \"--port\" ] || [ -z \"$2\" ]; then\n"
        "  echo 'Usage: sh flash.sh --port PORT' >&2\n"
        "  exit 2\n"
        "fi\n"
        "PORT=$2\n"
        "case \"$PORT\" in -*|*[!A-Za-z0-9._/:+-]*)\n"
        "  echo 'flash.sh: unsafe port value' >&2\n"
        "  exit 2\n"
        "esac\n"
        "DIR=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\n"
        f"python -m esptool --port \"$PORT\" --chip {CHIP} --baud {DEFAULT_BAUD} "
        f"write_flash {option_text}"
        + " ".join(
            f"{item['offset']} \"$DIR/{item['archive_path']}\""
            for item in sorted(records, key=lambda x: offset(x["offset"]))
        )
        + "\n"
    )
    batch = (
        "@echo off\r\n"
        "setlocal\r\n"
        "if not \"%~1\"==\"--port\" goto usage\r\n"
        "if \"%~2\"==\"\" goto usage\r\n"
        "if not \"%3\"==\"\" goto usage\r\n"
        "set \"PORT=%~2\"\r\n"
        "if \"%PORT:~0,1%\"==\"-\" goto usage\r\n"
        "if not \"%PORT:#=%\"==\"%PORT%\" goto usage\r\n"
        "for /f \"eol=# delims=ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._/:+-\" %%A in (\"%PORT%\") do goto usage\r\n"
        f"python -m esptool --port \"%PORT%\" --chip {CHIP} --baud {DEFAULT_BAUD} "
        f"write_flash {option_text}"
        + " ".join(
            f"{item['offset']} \"%~dp0{str(item['archive_path']).replace('/', chr(92))}\""
            for item in sorted(records, key=lambda x: offset(x["offset"]))
        )
        + "\r\nif errorlevel 1 exit /b %errorlevel%\r\n"
        "exit /b 0\r\n"
        ":usage\r\n"
        "echo Usage: flash.cmd --port PORT 1>&2\r\n"
        "exit /b 2\r\n"
    )
    return command, shell, batch


def manifest(framework: str, version: str, project: Path, records: list[dict[str, object]], command: str, profile: str, fqbn: str | None = None, source_sha: str | None = None) -> dict[str, object]:
    trusted_root, trusted_head = trusted_package_context()
    if source_sha is not None and source_sha != trusted_head:
        raise ValueError("Manifest source SHA differs from the clean trusted repository HEAD")
    contract = BOARD_PROFILES[profile]
    document: dict[str, object] = {"schema_version": 1, "board": BOARD, "chip": CHIP, "board_profile": profile, "chip_revision": {"minimum": contract["minimum"], "maximum_exclusive": contract["maximum_exclusive"], "maximum_note": contract["maximum_note"]}, "c6_firmware_included": False, "framework": framework, "framework_version": version, "source_project": repository_relative(project, trusted_root), "git_sha": trusted_head, "generated_at_utc": datetime.now(timezone.utc).isoformat(), "flash": {"baud": DEFAULT_BAUD, "size_bytes": FLASH_SIZE, "segmented_bytes": sum(int(record["size"]) for record in records), "command": command}, "files": records}
    if fqbn:
        document["fqbn"] = fqbn
    return document


def _safe_archive_path(value: str) -> bool:
    path = PurePosixPath(value)
    return bool(
        path.parts
        and not path.is_absolute()
        and not value.startswith("//")
        and not re.match(r"^[A-Za-z]:", value)
        and ".." not in path.parts
        and "\\" not in value
        and not any(ord(character) < 32 for character in value)
        and path.as_posix() == value
    )


def _strict_json_object(payload: bytes, description: str) -> dict[str, object]:
    """Load one UTF-8 JSON object while rejecting duplicate keys at every level."""
    def object_without_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{description} contains a duplicate JSON key: {key}")
            result[key] = value
        return result

    try:
        value = json.loads(payload.decode("utf-8"), object_pairs_hook=object_without_duplicates)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"{description} is not valid UTF-8 JSON") from exc
    if not isinstance(value, dict):
        raise ValueError(f"{description} must be a JSON object")
    return value


def _contract_string(value: object, description: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{description} must be a non-empty string")
    return value


def _contract_positive_int(value: object, description: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
        raise ValueError(f"{description} must be a positive integer")
    return value


def _portable_helper_archive_path(value: str) -> bool:
    """Limit helper operands to paths that are unambiguous in POSIX and cmd.exe."""
    return _safe_archive_path(value) and bool(re.fullmatch(r"[A-Za-z0-9._/-]+", value))


def _validate_arduino_written_bundle(
    archive: zipfile.ZipFile,
    names: list[str],
    document: dict[str, object],
    records: list[dict[str, object]],
    trusted_head: str,
    trusted_root: Path,
) -> None:
    """Independently reconstruct the Arduino release contract from ZIP bytes."""
    expected_top_level_keys = {
        "schema_version", "board", "chip", "board_profile", "chip_revision",
        "c6_firmware_included", "framework", "framework_version", "source_project",
        "git_sha", "generated_at_utc", "flash", "files", "fqbn", "target", "bsp",
        "arduino_write_flash_args", "arduino_build_identity", "arduino_metadata",
    }
    generated_at_raw = document.get("generated_at_utc")
    try:
        generated_at = datetime.fromisoformat(
            _contract_string(generated_at_raw, "Arduino generated-at timestamp")
        )
    except ValueError as exc:
        raise ValueError("Generated Arduino ZIP has an invalid generated-at timestamp") from exc
    revision = document.get("chip_revision")
    if (set(document) != expected_top_level_keys
            or not isinstance(document.get("schema_version"), int)
            or isinstance(document.get("schema_version"), bool)
            or document.get("schema_version") != 1
            or document.get("c6_firmware_included") is not False
            or generated_at.tzinfo is None
            or generated_at.utcoffset() != timezone.utc.utcoffset(generated_at)
            or not isinstance(revision, dict)
            or set(revision) != {"minimum", "maximum_exclusive", "maximum_note"}
            or revision.get("minimum") != BOARD_PROFILES["rev3_x"]["minimum"]
            or revision.get("maximum_exclusive") != BOARD_PROFILES["rev3_x"]["maximum_exclusive"]
            or revision.get("maximum_note") != BOARD_PROFILES["rev3_x"]["maximum_note"]
            or document.get("framework") != "arduino-esp32"
            or document.get("framework_version") != ARDUINO_CORE_VERSION
            or document.get("fqbn") != ARDUINO_FQBN
            or document.get("target") != CHIP
            or document.get("board") != BOARD
            or document.get("chip") != CHIP
            or document.get("board_profile") != "rev3_x"):
        raise ValueError("Generated Arduino ZIP identity differs from the release contract")
    parse_arduino_fqbn(_contract_string(document.get("fqbn"), "Arduino manifest FQBN"))
    product_sha = _contract_string(document.get("git_sha"), "Arduino product Git SHA").lower()
    if (not re.fullmatch(r"[0-9a-f]{40}", product_sha)
            or document.get("git_sha") != product_sha
            or product_sha != trusted_head):
        raise ValueError("Generated Arduino ZIP requires a lowercase 40-hex product Git SHA")
    source_project = _contract_string(document.get("source_project"), "Arduino source project")
    if (not _portable_helper_archive_path(source_project)
            or not re.fullmatch(r"[A-Za-z0-9._-]+", PurePosixPath(source_project).name)):
        raise ValueError("Generated Arduino ZIP source project is unsafe")
    project_name = PurePosixPath(source_project).name
    try:
        recomputed_primary = primary_sketch_identity(trusted_root, source_project, trusted_head)
    except ValueError as exc:
        raise ValueError(
            "Generated Arduino ZIP source project is not the trusted primary sketch: "
            f"{exc}"
        ) from exc

    metadata_contract = document.get("arduino_metadata")
    expected_metadata_keys = {"path", "size", "sha256", "source"}
    if (not isinstance(metadata_contract, dict)
            or set(metadata_contract) != expected_metadata_keys
            or metadata_contract.get("path") != "metadata/flash_args"
            or metadata_contract.get("source") != "Arduino core 3.3.11 build-path output"):
        raise ValueError("Generated Arduino ZIP flash_args contract is incomplete")
    metadata_payload = archive.read("metadata/flash_args")
    metadata_size = _contract_positive_int(metadata_contract.get("size"), "flash_args size")
    metadata_sha = _contract_string(metadata_contract.get("sha256"), "flash_args SHA-256").lower()
    if (not re.fullmatch(r"[0-9a-f]{64}", metadata_sha)
            or len(metadata_payload) != metadata_size
            or sha256_bytes(metadata_payload) != metadata_sha):
        raise ValueError("Generated Arduino ZIP flash_args size or hash differs from its manifest")
    try:
        metadata_options, metadata_entries = parse_arduino_flash_args_text(
            metadata_payload.decode("utf-8")
        )
        manifest_options = validate_arduino_write_flash_args(
            document.get("arduino_write_flash_args")
        )
    except (UnicodeError, ValueError) as exc:
        raise ValueError(f"Generated Arduino ZIP flash_args is invalid: {exc}") from exc
    if manifest_options != metadata_options:
        raise ValueError("Generated Arduino ZIP write options differ from flash_args")
    if metadata_entries != sorted(metadata_entries, key=lambda entry: entry[0]):
        raise ValueError("Generated Arduino ZIP flash_args segments are not in offset order")
    full_flash_bytes = flash_size_bytes(metadata_options[5])

    identity_contract = document.get("arduino_build_identity")
    expected_identity_contract_keys = {
        "path", "size", "sha256", "source_name", "source_size", "source_sha256",
        "raw_archived",
    }
    if (not isinstance(identity_contract, dict)
            or set(identity_contract) != expected_identity_contract_keys
            or identity_contract.get("path") != "metadata/arduino_build_identity.json"
            or identity_contract.get("raw_archived") is not False):
        raise ValueError("Generated Arduino ZIP canonical build identity contract is incomplete")
    canonical_payload = archive.read("metadata/arduino_build_identity.json")
    canonical_size = _contract_positive_int(identity_contract.get("size"), "canonical identity size")
    canonical_sha = _contract_string(identity_contract.get("sha256"), "canonical identity SHA-256").lower()
    if (not re.fullmatch(r"[0-9a-f]{64}", canonical_sha)
            or len(canonical_payload) != canonical_size
            or sha256_bytes(canonical_payload) != canonical_sha):
        raise ValueError("Generated Arduino ZIP canonical identity size or hash differs from its manifest")
    canonical = _strict_json_object(canonical_payload, "Arduino canonical build identity")
    expected_canonical_keys = {
        "schema_version", "source_name", "source_size", "source_sha256", "fqbn", "core_version",
        "sketch_path", "sketch_name", "application_metadata_path", "primary_source",
    }
    canonical_source_size = _contract_positive_int(
        canonical.get("source_size"), "raw build.options.json size"
    )
    canonical_source_sha = _contract_string(
        canonical.get("source_sha256"), "raw build.options.json SHA-256"
    ).lower()
    if (set(canonical) != expected_canonical_keys
            or not isinstance(canonical.get("schema_version"), int)
            or isinstance(canonical.get("schema_version"), bool)
            or canonical.get("schema_version") != 1
            or canonical.get("source_name") != "build.options.json"
            or not re.fullmatch(r"[0-9a-f]{64}", canonical_source_sha)
            or canonical.get("source_sha256") != canonical_source_sha
            or canonical.get("fqbn") != ARDUINO_FQBN
            or canonical.get("core_version") != ARDUINO_CORE_VERSION
            or canonical.get("sketch_path") != source_project
            or canonical.get("sketch_name") != project_name
            or not _portable_helper_archive_path(_contract_string(
                canonical.get("application_metadata_path"),
                "Arduino canonical application metadata path",
            ))
            or identity_contract.get("source_name") != canonical.get("source_name")
            or identity_contract.get("source_size") != canonical_source_size
            or identity_contract.get("source_sha256") != canonical_source_sha):
        raise ValueError("Generated Arduino ZIP raw/canonical build identity evidence differs")
    primary_source = canonical.get("primary_source")
    if (
        not isinstance(primary_source, dict)
        or set(primary_source) != {"path", "size", "sha256", "git_blob_sha"}
        or primary_source.get("path") != recomputed_primary["path"]
        or primary_source.get("size") != recomputed_primary["size"]
        or primary_source.get("sha256") != recomputed_primary["sha256"]
        or primary_source.get("git_blob_sha") != recomputed_primary["git_blob_sha"]
    ):
        raise ValueError("Generated Arduino ZIP primary-source identity differs from the trusted HEAD tree")

    bsp = document.get("bsp")
    expected_bsp_keys = {
        "component", "version", "source_git_sha", "component_git_tree_sha",
        "applicable", "note",
    }
    if (not isinstance(bsp, dict)
            or set(bsp) != expected_bsp_keys
            or bsp.get("component") != ARDUINO_BSP_COMPONENT
            or bsp.get("applicable") is not False
            or bsp.get("note") != "Release evidence binding only; Arduino uses the pinned core and repository display library."):
        raise ValueError("Generated Arduino ZIP BSP evidence binding is incomplete")
    bsp_version, bsp_source_sha, bsp_tree_sha = validate_bsp_binding(
        _contract_string(bsp.get("version"), "Arduino BSP version"),
        _contract_string(bsp.get("source_git_sha"), "Arduino BSP source Git SHA"),
        _contract_string(bsp.get("component_git_tree_sha"), "Arduino BSP component tree SHA"),
    )
    if (bsp.get("source_git_sha") != bsp_source_sha
            or bsp.get("component_git_tree_sha") != bsp_tree_sha):
        raise ValueError("Generated Arduino ZIP BSP hashes must use canonical lowercase hex")

    expected_record_keys = {
        "offset", "archive_path", "metadata_path", "metadata_source", "role", "size", "sha256",
        "target", "fqbn", "framework_version", "product_git_sha", "bsp_component", "bsp_version",
        "bsp_source_git_sha", "bsp_component_git_tree_sha", "bsp_applicable",
    }
    actual_entries: list[tuple[int, str]] = []
    roles: list[str] = []
    archive_paths: set[str] = set()
    validated_records: list[dict[str, object]] = []
    for record in records:
        if set(record) != expected_record_keys:
            raise ValueError("Generated Arduino ZIP segment record has unknown or missing fields")
        raw_offset = offset(record.get("offset"))
        if record.get("offset") != f"0x{raw_offset:x}":
            raise ValueError("Generated Arduino ZIP segment offset is not canonical hexadecimal")
        metadata_path = _contract_string(record.get("metadata_path"), "Arduino metadata path")
        if not _safe_archive_path(metadata_path):
            raise ValueError("Generated Arduino ZIP metadata path is unsafe")
        archive_path = _contract_string(record.get("archive_path"), "Arduino archive path")
        if (not _portable_helper_archive_path(archive_path)
                or not archive_path.startswith("bin/")
                or archive_path in archive_paths
                or _arduino_whole_flash_name(archive_path)):
            raise ValueError("Generated Arduino ZIP archive path is unsafe or duplicated")
        archive_paths.add(archive_path)
        role = arduino_segment_role(metadata_path, project_name)
        roles.append(role)
        segment_size = _contract_positive_int(record.get("size"), "Arduino segment size")
        segment_sha = _contract_string(record.get("sha256"), "Arduino segment SHA-256").lower()
        if not re.fullmatch(r"[0-9a-f]{64}", segment_sha) or record.get("sha256") != segment_sha:
            raise ValueError("Generated Arduino ZIP segment SHA-256 is not canonical")
        payload = archive.read(archive_path)
        if len(payload) != segment_size or sha256_bytes(payload) != segment_sha:
            raise ValueError("Generated Arduino ZIP segment size or hash differs from its manifest")
        expected_provenance: dict[str, object] = {
            "metadata_source": "metadata/flash_args",
            "role": role,
            "target": CHIP,
            "fqbn": ARDUINO_FQBN,
            "framework_version": ARDUINO_CORE_VERSION,
            "product_git_sha": product_sha,
            "bsp_component": ARDUINO_BSP_COMPONENT,
            "bsp_version": bsp_version,
            "bsp_source_git_sha": bsp_source_sha,
            "bsp_component_git_tree_sha": bsp_tree_sha,
            "bsp_applicable": False,
        }
        if any(record.get(key) != value for key, value in expected_provenance.items()):
            raise ValueError("Generated Arduino ZIP segment provenance differs from its package identity")
        actual_entries.append((raw_offset, metadata_path))
        validated_records.append(record)
    if actual_entries != metadata_entries:
        raise ValueError("Generated Arduino ZIP segments differ from packaged flash_args")
    if any(roles.count(role) != 1 for role in ("bootloader", "partition_table", "application")):
        raise ValueError("Generated Arduino ZIP must contain one bootloader, partition table, and application")
    if roles.count("boot_app0") > 1:
        raise ValueError("Generated Arduino ZIP contains duplicate boot_app0 segments")
    application_metadata_paths = [
        metadata_path
        for (_, metadata_path), role in zip(actual_entries, roles)
        if role == "application"
    ]
    if application_metadata_paths != [canonical.get("application_metadata_path")]:
        raise ValueError("Generated Arduino ZIP application output differs from its sketch identity")
    validate_plan(validated_records, full_flash_bytes)

    flash = document.get("flash")
    expected_flash_keys = {
        "baud", "size_bytes", "segmented_bytes", "command", "full_flash_bytes",
        "segmented_payload_total", "segmented_fraction",
    }
    if not isinstance(flash, dict) or set(flash) != expected_flash_keys:
        raise ValueError("Generated Arduino ZIP flash contract is missing")
    payload_total = sum(_contract_positive_int(record.get("size"), "Arduino segment size") for record in records)
    expected_fraction = round(payload_total / full_flash_bytes, 8)
    expected_command, expected_shell, expected_batch = flash_helpers(validated_records, metadata_options)
    flash_baud = _contract_positive_int(flash.get("baud"), "Arduino flash baud")
    flash_capacity = _contract_positive_int(flash.get("size_bytes"), "Arduino flash capacity")
    recorded_full_flash = _contract_positive_int(
        flash.get("full_flash_bytes"), "Arduino full flash bytes"
    )
    recorded_segmented = _contract_positive_int(
        flash.get("segmented_bytes"), "Arduino segmented bytes"
    )
    recorded_payload = _contract_positive_int(
        flash.get("segmented_payload_total"), "Arduino segmented payload total"
    )
    recorded_fraction = flash.get("segmented_fraction")
    if (flash_baud != DEFAULT_BAUD
            or flash_capacity != FLASH_SIZE
            or recorded_full_flash != full_flash_bytes
            or recorded_segmented != payload_total
            or recorded_payload != payload_total
            or not isinstance(recorded_fraction, float)
            or recorded_fraction != expected_fraction
            or payload_total > full_flash_bytes // 2
            or flash.get("command") != expected_command):
        raise ValueError("Generated Arduino ZIP flash command, geometry, or totals differ from metadata")
    if (archive.read("flash.sh") != expected_shell.encode("utf-8")
            or archive.read("flash.cmd") != expected_batch.encode("utf-8")):
        raise ValueError("Generated Arduino ZIP flash helpers differ from the verified metadata plan")

    expected_inventory = {
        "manifest.json", "flash.sh", "flash.cmd", "metadata/flash_args",
        "metadata/arduino_build_identity.json", *archive_paths,
    }
    if set(names) != expected_inventory:
        raise ValueError("Generated Arduino ZIP file inventory is not exact")


class _DirectoryBundleReader:
    """Minimal ZipFile-compatible reader for an already safely extracted package."""

    def __init__(self, root: Path) -> None:
        self.root = root.resolve()

    def read(self, name: str) -> bytes:
        if not _safe_archive_path(name):
            raise ValueError("Extracted Arduino package contains an unsafe path")
        path = contained_without_symlinks(
            self.root,
            self.root.joinpath(*PurePosixPath(name).parts),
            f"Extracted Arduino package member {name}",
        )
        return path.read_bytes()


def validate_arduino_extracted_bundle(package: Path, expected_head: str) -> dict[str, object]:
    """Re-read and strictly validate an extracted Arduino package against this checkout."""
    trusted_root, trusted_head = trusted_repository()
    if expected_head != trusted_head or not re.fullmatch(r"[0-9a-f]{40}", expected_head):
        raise ValueError("Extracted Arduino package is not bound to the trusted repository HEAD")
    package = package.resolve()
    if not package.is_dir() or package.is_symlink():
        raise ValueError("Extracted Arduino package directory is missing or symlinked")
    names: list[str] = []
    for path in package.rglob("*"):
        if path.is_symlink():
            raise ValueError("Extracted Arduino package contains a symlink")
        if path.is_file():
            name = path.relative_to(package).as_posix()
            if not _safe_archive_path(name):
                raise ValueError("Extracted Arduino package contains an unsafe path")
            names.append(name)
    if len(names) != len(set(names)):
        raise ValueError("Extracted Arduino package contains duplicate paths")
    reader = _DirectoryBundleReader(package)
    document = _strict_json_object(reader.read("manifest.json"), "Extracted Arduino manifest")
    records_raw = document.get("files")
    if not isinstance(records_raw, list) or not records_raw or not all(
        isinstance(record, dict) for record in records_raw
    ):
        raise ValueError("Extracted Arduino manifest has no valid segment records")
    records = records_raw
    if any("build.options" in name.casefold() or "expanded" in name.casefold() for name in names):
        raise ValueError("Extracted Arduino package contains raw build properties")
    if any(_arduino_whole_flash_name(name) for name in names):
        raise ValueError("Extracted Arduino package contains a merged or whole-flash image")
    reject_private_artifact_bytes(
        "\n".join(names).encode("utf-8"), "Extracted Arduino package names"
    )
    _validate_arduino_written_bundle(reader, names, document, records, trusted_head, trusted_root)  # type: ignore[arg-type]
    for name in names:
        if not is_binary_payload(name):
            reject_private_artifact_bytes(
                reader.read(name), f"Extracted Arduino package member {name}"
            )
    return document


def validate_written_bundle(
    output: Path,
    document: dict[str, object],
    extra_names: list[str],
    trusted_head: str,
    trusted_root: Path,
) -> None:
    if document.get("git_sha") != trusted_head:
        raise ValueError("Generated firmware ZIP product SHA differs from trusted repository HEAD")
    records_raw = document.get("files")
    if not isinstance(records_raw, list) or not records_raw or not all(
        isinstance(record, dict) for record in records_raw
    ):
        raise ValueError("Generated firmware ZIP manifest has no valid segment records")
    records = records_raw
    is_arduino = document.get("framework") == "arduino-esp32"
    expected = {str(record["archive_path"]) for record in records if isinstance(record, dict)}
    if is_arduino:
        required_extras = {"metadata/flash_args", "metadata/arduino_build_identity.json"}
        if len(extra_names) != len(required_extras) or set(extra_names) != required_extras:
            raise ValueError("Generated Arduino ZIP extras differ from the release contract")
        expected.update(required_extras)
    else:
        expected.update(extra_names)
    expected.update({"manifest.json", "flash.sh", "flash.cmd"})
    with zipfile.ZipFile(output) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or set(names) != expected or any(not _safe_archive_path(name) for name in names):
            raise ValueError("Generated firmware ZIP has an unsafe, duplicate, missing, or unexpected path")
        if is_arduino and any("build.options" in name.casefold() or "expanded" in name.casefold() for name in names):
            raise ValueError("Generated firmware ZIP contains raw build properties")
        if is_arduino:
            reject_private_artifact_bytes("\n".join(names).encode("utf-8"), "Generated firmware ZIP names")
        if is_arduino and any(_arduino_whole_flash_name(name) for name in names):
            raise ValueError("Generated firmware ZIP contains a merged or whole-flash image")
        if archive.testzip() is not None:
            raise ValueError("Generated firmware ZIP failed its CRC check")
        archived_manifest = _strict_json_object(
            archive.read("manifest.json"), "Generated firmware ZIP manifest"
        )
        if archived_manifest != document:
            raise ValueError("Generated firmware ZIP manifest differs from the package plan")
        if is_arduino:
            _validate_arduino_written_bundle(
                archive, names, archived_manifest, records, trusted_head, trusted_root
            )
        for contract_name in ("arduino_metadata", "arduino_build_identity"):
            contract = document.get(contract_name)
            if contract is not None:
                if not isinstance(contract, dict):
                    raise ValueError(f"{contract_name} contract must be an object")
                metadata_name = str(contract.get("path", ""))
                if metadata_name not in names or not _safe_archive_path(metadata_name):
                    raise ValueError(f"{contract_name} points outside the package")
                metadata_payload = archive.read(metadata_name)
                if (len(metadata_payload) != contract.get("size")
                        or sha256_bytes(metadata_payload) != contract.get("sha256")):
                    raise ValueError(f"Packaged {contract_name} size or hash differs from its manifest")
        for name in names:
            payload = archive.read(name)
            if is_arduino and not is_binary_payload(name):
                reject_private_artifact_bytes(payload, f"Generated firmware ZIP member {name}")
        for record in records:
            assert isinstance(record, dict)
            payload = archive.read(str(record["archive_path"]))
            if len(payload) != record["size"] or sha256_bytes(payload) != record["sha256"]:
                raise ValueError("Generated firmware ZIP segment size or hash differs from its manifest")
        flash = document.get("flash")
        if not isinstance(flash, dict):
            raise ValueError("Generated firmware ZIP flash contract is missing")
        payload_total = sum(int(record["size"]) for record in records if isinstance(record, dict))
        if (flash.get("segmented_bytes") != payload_total
                or (document.get("framework") == "arduino-esp32"
                    and flash.get("segmented_payload_total") != payload_total)):
            raise ValueError("Generated firmware ZIP segmented payload totals differ from its files")
        if not is_arduino:
            expected_command, expected_shell, expected_batch = flash_helpers(records)
            if (flash.get("command") != expected_command
                    or archive.read("flash.sh") != expected_shell.encode("utf-8")
                    or archive.read("flash.cmd") != expected_batch.encode("utf-8")):
                raise ValueError("Generated firmware ZIP flash helpers differ from its manifest plan")


def write_bundle(
    output: Path,
    records: list[dict[str, object]],
    sources: list[Path],
    document: dict[str, object],
    extras: list[tuple[Path, str]] | None = None,
    write_flash_args: list[str] | None = None,
    generated_extras: list[tuple[str, bytes]] | None = None,
    *,
    trusted_head: str,
    trusted_root: Path,
) -> Path:
    if output.exists():
        raise FileExistsError(f"Refusing to overwrite package: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    extras = list(extras or [])
    generated_extras = list(generated_extras or [])
    if (len(sources) != len(records) or any(not _safe_archive_path(name) for _, name in extras)
            or any(not _safe_archive_path(name) for name, _ in generated_extras)):
        raise ValueError("Package source and archive metadata do not match")
    _, shell, batch = flash_helpers(records, write_flash_args)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            prefix=f".{output.name}.", suffix=".tmp", dir=output.parent, delete=False
        ) as handle:
            temporary = Path(handle.name)
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for source, record in zip(sources, records): archive.write(source, str(record["archive_path"]))
            for source, name in extras: archive.write(source, name)
            for name, payload in generated_extras: archive.writestr(name, payload)
            archive.writestr("manifest.json", json.dumps(document, indent=2, sort_keys=True) + "\n")
            archive.writestr("flash.sh", shell)
            archive.writestr("flash.cmd", batch)
        validate_written_bundle(
            temporary,
            document,
            [name for _, name in extras] + [name for name, _ in generated_extras],
            trusted_head,
            trusted_root,
        )
        if output.exists():
            raise FileExistsError(f"Refusing to overwrite package: {output}")
        os.replace(temporary, output)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return output


def package_idf(project: Path, build: Path, version: str, output: Path, profile: str) -> Path:
    trusted_root, trusted_head = trusted_package_context()
    project, build = project.resolve(), build.resolve()
    repository_relative(project, trusted_root)
    if not project.is_dir():
        raise ValueError("ESP-IDF project is missing from the trusted repository")
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
    return write_bundle(
        output, records, sources, document, [(args_path, "metadata/flasher_args.json")],
        trusted_head=trusted_head, trusted_root=trusted_root,
    )


def package_arduino(
    project: Path,
    build: Path,
    version: str,
    output: Path,
    fqbn: str,
    profile: str,
    bsp_version: str,
    bsp_source_git_sha: str,
    bsp_component_tree_sha: str,
) -> Path:
    if board_profile(profile) != "rev3_x":
        raise ValueError("Arduino ChipVariant=postv3 packages must use board profile rev3_x")
    if version != ARDUINO_CORE_VERSION:
        raise ValueError(f"Arduino packages must use pinned core {ARDUINO_CORE_VERSION}")
    parse_arduino_fqbn(fqbn)
    bsp_version, bsp_source_git_sha, bsp_component_tree_sha = validate_bsp_binding(
        bsp_version, bsp_source_git_sha, bsp_component_tree_sha
    )
    trusted_root, product_sha = trusted_package_context()
    project, build = project.resolve(), build.resolve()
    sketch_path = repository_relative(project, trusted_root)
    if not project.is_dir():
        raise ValueError("Arduino sketch directory is missing from the trusted repository")
    primary_identity = primary_sketch_identity(trusted_root, sketch_path, product_sha)
    build_options_path = contained_without_symlinks(
        build, build / "build.options.json", "Arduino build identity metadata"
    )
    try:
        raw_build_options = build_options_path.read_bytes()
    except OSError as exc:
        raise ValueError("Arduino build.options.json is invalid") from exc
    build_options = _strict_json_object(raw_build_options, "Arduino build.options.json")
    if not isinstance(build_options, dict) or build_options.get("fqbn") != fqbn:
        raise ValueError("Arduino build.options.json FQBN differs from the requested package FQBN")
    sketch_location = build_options.get("sketchLocation")
    if not isinstance(sketch_location, str) or not sketch_location or not Path(sketch_location).is_absolute():
        raise ValueError("Arduino build.options.json has no absolute sketchLocation identity")
    resolved_sketch_location = Path(sketch_location).resolve()
    allowed_sketch_locations = {
        project.resolve(),
        (project / f"{project.name}.ino").resolve(),
    }
    if resolved_sketch_location not in allowed_sketch_locations:
        raise ValueError("Arduino build.options.json sketchLocation differs from the requested sketch")
    if (not _portable_helper_archive_path(sketch_path)
            or not re.fullmatch(r"[A-Za-z0-9._-]+", project.name)):
        raise ValueError("Arduino sketch path cannot be represented as a safe repository-relative identity")
    hardware_folders = build_options.get("hardwareFolders")
    if not isinstance(hardware_folders, str) or not hardware_folders:
        raise ValueError("Arduino build.options.json has no hardwareFolders core identity")
    normalized_hardware_folders = [
        item.strip().replace("\\", "/").rstrip("/")
        for item in hardware_folders.split(",")
        if item.strip()
    ]
    expected_core_suffix = f"/esp32/{version}"
    if not normalized_hardware_folders or any(
        not item.endswith(expected_core_suffix) for item in normalized_hardware_folders
    ):
        raise ValueError("Arduino build.options.json hardwareFolders differs from the pinned core version")
    metadata_path = contained_without_symlinks(build, build / "flash_args", "Arduino flash metadata")
    metadata_text = metadata_path.read_text(encoding="utf-8")
    write_flash_args, entries = parse_arduino_flash_args_text(metadata_text)
    used: set[str] = set()
    records: list[dict[str, object]] = []
    sources: list[Path] = []
    roles: list[str] = []
    for raw_offset, metadata_relative in entries:
        relative = PurePosixPath(metadata_relative)
        source = contained_without_symlinks(
            build,
            build.joinpath(*relative.parts),
            "Arduino flash binary",
        )
        role = arduino_segment_role(metadata_relative, project.name)
        roles.append(role)
        records.append(
            {
                "offset": f"0x{raw_offset:x}",
                "archive_path": archive_name(source, used),
                "metadata_path": metadata_relative,
                "metadata_source": "metadata/flash_args",
                "role": role,
                "size": source.stat().st_size,
                "sha256": sha256(source),
                "target": CHIP,
                "fqbn": fqbn,
                "framework_version": version,
                "product_git_sha": product_sha,
                "bsp_component": ARDUINO_BSP_COMPONENT,
                "bsp_version": bsp_version,
                "bsp_source_git_sha": bsp_source_git_sha,
                "bsp_component_git_tree_sha": bsp_component_tree_sha,
                "bsp_applicable": False,
            }
        )
        sources.append(source)
    for required_role in ("bootloader", "partition_table", "application"):
        if roles.count(required_role) != 1:
            raise ValueError(f"Arduino flash_args must contain exactly one {required_role} segment")
    if roles.count("boot_app0") > 1:
        raise ValueError("Arduino flash_args contains duplicate boot_app0 segments")
    application_metadata_path = next(
        str(record["metadata_path"])
        for record in records
        if record["role"] == "application"
    )
    if not _portable_helper_archive_path(application_metadata_path):
        raise ValueError("Arduino application path cannot be represented as a safe relative identity")
    validate_plan(records, flash_size_bytes(write_flash_args[5]))
    segmented_payload_total = sum(int(record["size"]) for record in records)
    if segmented_payload_total > FLASH_SIZE // 2:
        raise ValueError("Arduino segmented payload exceeds the one-half-flash release threshold")
    command, _, _ = flash_helpers(records, write_flash_args)
    document = manifest(
        "arduino-esp32",
        version,
        project,
        records,
        command,
        profile,
        fqbn,
        product_sha,
    )
    document["target"] = CHIP
    document["bsp"] = {
        "component": ARDUINO_BSP_COMPONENT,
        "version": bsp_version,
        "source_git_sha": bsp_source_git_sha,
        "component_git_tree_sha": bsp_component_tree_sha,
        "applicable": False,
        "note": "Release evidence binding only; Arduino uses the pinned core and repository display library.",
    }
    document["arduino_write_flash_args"] = write_flash_args
    canonical_identity = {
        "schema_version": 1,
        "source_name": "build.options.json",
        "source_size": len(raw_build_options),
        "source_sha256": sha256_bytes(raw_build_options),
        "fqbn": fqbn,
        "core_version": version,
        "sketch_path": sketch_path,
        "sketch_name": project.name,
        "application_metadata_path": application_metadata_path,
        "primary_source": primary_identity,
    }
    canonical_identity_payload = (
        json.dumps(canonical_identity, sort_keys=True, separators=(",", ":")) + "\n"
    ).encode("utf-8")
    document["arduino_build_identity"] = {
        "path": "metadata/arduino_build_identity.json",
        "size": len(canonical_identity_payload),
        "sha256": sha256_bytes(canonical_identity_payload),
        "source_name": canonical_identity["source_name"],
        "source_size": canonical_identity["source_size"],
        "source_sha256": canonical_identity["source_sha256"],
        "raw_archived": False,
    }
    document["arduino_metadata"] = {
        "path": "metadata/flash_args",
        "size": metadata_path.stat().st_size,
        "sha256": sha256(metadata_path),
        "source": "Arduino core 3.3.11 build-path output",
    }
    document["flash"]["full_flash_bytes"] = flash_size_bytes(write_flash_args[5])
    document["flash"]["segmented_payload_total"] = segmented_payload_total
    document["flash"]["segmented_fraction"] = round(
        int(document["flash"]["segmented_bytes"]) / int(document["flash"]["full_flash_bytes"]),
        8,
    )
    return write_bundle(
        output,
        records,
        sources,
        document,
        [(metadata_path, "metadata/flash_args")],
        write_flash_args,
        [("metadata/arduino_build_identity.json", canonical_identity_payload)],
        trusted_head=product_sha,
        trusted_root=trusted_root,
    )


def main() -> int:
    parser = argparse.ArgumentParser(); subs = parser.add_subparsers(dest="kind", required=True)
    for kind in ("esp-idf", "arduino"):
        sub = subs.add_parser(kind); sub.add_argument("--project", type=Path, required=True); sub.add_argument("--build-dir", type=Path, required=True); sub.add_argument("--framework-version", required=True); sub.add_argument("--board-profile", choices=tuple(BOARD_PROFILES), required=True); sub.add_argument("--output", type=Path, required=True)
        if kind == "arduino":
            sub.add_argument("--fqbn", required=True)
            sub.add_argument("--bsp-version", required=True)
            sub.add_argument("--bsp-source-git-sha", required=True)
            sub.add_argument("--bsp-component-tree-sha", required=True)
    args = parser.parse_args()
    output = package_idf(args.project, args.build_dir, args.framework_version, args.output, args.board_profile) if args.kind == "esp-idf" else package_arduino(args.project, args.build_dir, args.framework_version, args.output, args.fqbn, args.board_profile, args.bsp_version, args.bsp_source_git_sha, args.bsp_component_tree_sha)
    print(output.as_posix()); return 0


if __name__ == "__main__": raise SystemExit(main())
