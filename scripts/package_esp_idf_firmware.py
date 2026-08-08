#!/usr/bin/env python3
"""Create a self-contained flash package from an ESP-IDF build directory."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tempfile
import zipfile


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Package every image referenced by flasher_args.json."
    )
    parser.add_argument("build_dir", type=Path, help="ESP-IDF build directory")
    parser.add_argument(
        "--output-dir", type=Path, default=Path("release-artifacts"),
        help="Directory that receives the ZIP archive",
    )
    parser.add_argument("--project", help="Override the project name")
    parser.add_argument("--idf-version", help="Override the ESP-IDF version label")
    parser.add_argument("--git-sha", help="Source revision recorded in manifest.json")
    parser.add_argument("--baud", type=int, default=460800, help="Default flash baud")
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Replace an existing archive with the same project/revision label",
    )
    return parser.parse_args()


def load_json(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise SystemExit(f"Required file is missing: {path}") from exc
    except json.JSONDecodeError as exc:
        raise SystemExit(f"Invalid JSON in {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise SystemExit(f"JSON document must contain an object: {path}")
    return data


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_build_file(build_dir: Path, raw_name: str) -> tuple[Path, PurePosixPath]:
    relative = PurePosixPath(raw_name.replace("\\", "/"))
    if relative.is_absolute() or ".." in relative.parts:
        raise SystemExit(f"Unsafe flash file path: {raw_name}")
    source = (build_dir / Path(*relative.parts)).resolve()
    try:
        source.relative_to(build_dir)
    except ValueError as exc:
        raise SystemExit(f"Flash file escapes build directory: {raw_name}") from exc
    if not source.is_file():
        raise SystemExit(f"Flash image is missing: {source}")
    return source, relative


def slug(value: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9._-]+", "-", value).strip("-.")
    return normalized or "firmware"


def detect_git_sha(project_dir: Path) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(project_dir), "rev-parse", "--verify", "HEAD"],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError:
        return "unknown"
    candidate = result.stdout.strip()
    return candidate if result.returncode == 0 and candidate else "unknown"


def detect_project_path(build_dir: Path, description: dict) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(build_dir), "rev-parse", "--show-toplevel"],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError:
        result = None

    raw_project_path = description.get("project_path")
    safe_fallback = (
        PurePosixPath(raw_project_path.replace("\\", "/")).name
        if isinstance(raw_project_path, str) and raw_project_path
        else build_dir.parent.name
    )
    if result is None or result.returncode != 0 or not result.stdout.strip():
        return safe_fallback
    if not isinstance(raw_project_path, str) or not raw_project_path:
        return safe_fallback

    repository_root = Path(result.stdout.strip()).resolve()
    try:
        return Path(raw_project_path).resolve().relative_to(repository_root).as_posix()
    except ValueError:
        return safe_fallback


def build_esptool_command(extra: dict, target: str, baud: int) -> list[str]:
    if not isinstance(extra, dict):
        raise SystemExit("extra_esptool_args must be a JSON object")

    supported = {"chip", "before", "after", "stub"}
    unknown = sorted(set(extra) - supported)
    if unknown:
        raise SystemExit(
            "Unsupported extra_esptool_args fields: " + ", ".join(unknown)
        )

    chip = extra.get("chip", target)
    before = extra.get("before", "default_reset")
    after = extra.get("after", "hard_reset")
    for name, value in (("chip", chip), ("before", before), ("after", after)):
        if not isinstance(value, str) or not value:
            raise SystemExit(f"extra_esptool_args.{name} must be a non-empty string")

    stub = extra.get("stub", True)
    if not isinstance(stub, bool):
        raise SystemExit("extra_esptool_args.stub must be a boolean")

    command = [
        "python", "-m", "esptool",
        "--chip", chip,
        "-b", str(baud),
        "--before", before,
        "--after", after,
    ]
    if not stub:
        command.append("--no-stub")
    command.append("write_flash")
    return command


def add_zip_file(archive: zipfile.ZipFile, source: Path, archive_path: PurePosixPath) -> None:
    info = zipfile.ZipInfo.from_file(source, arcname=archive_path.as_posix())
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3
    mode = 0o100755 if source.name in {"flash.py", "flash.sh"} else 0o100644
    info.external_attr = mode << 16
    with source.open("rb") as input_file, archive.open(info, "w", force_zip64=True) as output_file:
        shutil.copyfileobj(input_file, output_file)


FLASH_RUNNER = r'''#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


manifest_path = Path(__file__).with_name("manifest.json")
try:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
except (OSError, json.JSONDecodeError) as exc:
    raise SystemExit(f"Cannot read a valid manifest.json: {exc}") from exc
if not isinstance(manifest, dict) or manifest.get("package_format") != 2:
    raise SystemExit("manifest.json is not a supported package-format 2 manifest")

root = manifest_path.parent.resolve()
files = manifest.get("files")
if not isinstance(files, list) or not files:
    raise SystemExit("manifest.json does not contain a non-empty files array")

expected_pairs = []
seen_offsets = set()
seen_paths = set()
for entry in files:
    if not isinstance(entry, dict):
        raise SystemExit("manifest.json contains an invalid file entry")

    offset = entry.get("offset")
    raw_path = entry.get("path")
    expected_size = entry.get("size")
    expected_hash = entry.get("sha256")
    if not isinstance(offset, str):
        raise SystemExit("manifest.json contains a non-string flash offset")
    try:
        numeric_offset = int(offset, 0)
    except ValueError as exc:
        raise SystemExit(f"manifest.json contains an invalid flash offset: {offset}") from exc
    if numeric_offset < 0 or numeric_offset in seen_offsets:
        raise SystemExit(f"manifest.json contains a duplicate or negative flash offset: {offset}")
    seen_offsets.add(numeric_offset)

    if not isinstance(raw_path, str) or not raw_path or "\\" in raw_path:
        raise SystemExit("manifest.json contains an invalid flash file path")
    relative = PurePosixPath(raw_path)
    if (
        relative.is_absolute()
        or not relative.parts
        or relative.parts[0] != "bin"
        or ".." in relative.parts
        or raw_path in seen_paths
    ):
        raise SystemExit(f"manifest.json contains an unsafe or duplicate path: {raw_path}")
    seen_paths.add(raw_path)
    candidate = (root / Path(*relative.parts)).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise SystemExit(f"Flash file escapes the package directory: {raw_path}") from exc
    if not candidate.is_file():
        raise SystemExit(f"Flash file is missing: {raw_path}")

    if isinstance(expected_size, bool) or not isinstance(expected_size, int) or expected_size < 0:
        raise SystemExit(f"manifest.json contains an invalid size for {raw_path}")
    if candidate.stat().st_size != expected_size:
        raise SystemExit(f"Flash file size mismatch: {raw_path}")
    if (
        not isinstance(expected_hash, str)
        or len(expected_hash) != 64
        or any(character not in "0123456789abcdefABCDEF" for character in expected_hash)
    ):
        raise SystemExit(f"manifest.json contains an invalid SHA-256 for {raw_path}")
    if sha256_file(candidate) != expected_hash.lower():
        raise SystemExit(f"Flash file SHA-256 mismatch: {raw_path}")
    expected_pairs.extend([offset, raw_path])

command = manifest.get("flash_command")
if not isinstance(command, list) or not command or not all(isinstance(item, str) for item in command):
    raise SystemExit("manifest.json does not contain a valid flash_command array")
if command[:3] != ["python", "-m", "esptool"]:
    raise SystemExit("manifest.json contains an unexpected flash command")
if command[-len(expected_pairs):] != expected_pairs:
    raise SystemExit("flash_command does not match the validated file list")
try:
    write_index = command.index("write_flash")
except ValueError as exc:
    raise SystemExit("flash_command is missing write_flash") from exc
if write_index >= len(command) - len(expected_pairs):
    raise SystemExit("flash_command places write_flash after the validated file list")
subprocess.run([sys.executable, *command[1:]], cwd=manifest_path.parent, check=True)
'''


def main() -> int:
    args = parse_args()
    if args.baud <= 0:
        raise SystemExit("--baud must be a positive integer")
    build_dir = args.build_dir.resolve()
    flasher = load_json(build_dir / "flasher_args.json")
    description_path = build_dir / "project_description.json"
    description = load_json(description_path) if description_path.is_file() else {}

    extra = flasher.get("extra_esptool_args", {})
    if not isinstance(extra, dict):
        raise SystemExit("extra_esptool_args must be a JSON object")
    project = args.project or description.get("project_name") or build_dir.parent.name
    target = description.get("target") or extra.get("chip") or "esp32"
    idf_version = args.idf_version or description.get("git_revision") or "unknown-idf"
    git_sha = (
        args.git_sha
        or os.environ.get("GITHUB_SHA")
        or detect_git_sha(build_dir.parent)
    )
    revision_label = slug(git_sha[:12] if git_sha != "unknown" else "unknown")
    package_name = slug(f"{project}-{target}-{idf_version}-{revision_label}")
    project_path = detect_project_path(build_dir, description)
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    archive_path = output_dir / f"{package_name}.zip"
    if archive_path.exists() and not args.overwrite:
        raise SystemExit(f"Archive already exists; use --overwrite to replace it: {archive_path}")

    flash_files = flasher.get("flash_files")
    if not isinstance(flash_files, dict) or not flash_files:
        raise SystemExit("flasher_args.json does not contain flash_files")

    raw_write_args = flasher.get("write_flash_args", [])
    if not isinstance(raw_write_args, list) or not all(isinstance(item, str) for item in raw_write_args):
        raise SystemExit("write_flash_args must be an array of strings")
    base_command = [*build_esptool_command(extra, target, args.baud), *raw_write_args]

    try:
        parsed_files = [
            (int(offset, 0), offset, raw_name)
            for offset, raw_name in flash_files.items()
        ]
    except (TypeError, ValueError) as exc:
        raise SystemExit("flash_files contains an invalid offset") from exc
    numeric_offsets = [numeric for numeric, _, _ in parsed_files]
    if any(numeric < 0 for numeric in numeric_offsets) or len(numeric_offsets) != len(set(numeric_offsets)):
        raise SystemExit("flash_files contains a duplicate or negative offset")
    ordered = [
        (offset, raw_name)
        for _, offset, raw_name in sorted(parsed_files, key=lambda item: item[0])
    ]
    manifest_files: list[dict] = []
    flash_pairs: list[str] = []
    destination_paths: set[str] = set()

    with tempfile.TemporaryDirectory(prefix=f"{package_name}-", dir=output_dir) as temp_name:
        package_dir = Path(temp_name) / package_name
        bin_dir = package_dir / "bin"
        bin_dir.mkdir(parents=True)

        for offset, raw_name in ordered:
            if not isinstance(raw_name, str) or not raw_name:
                raise SystemExit(f"flash_files contains an invalid file name at {offset}")
            source, relative = safe_build_file(build_dir, raw_name)
            destination_relative = PurePosixPath("bin") / relative
            destination_path = destination_relative.as_posix()
            if destination_path in destination_paths:
                raise SystemExit(f"flash_files contains a duplicate file path: {raw_name}")
            destination_paths.add(destination_path)
            destination = package_dir / Path(*destination_relative.parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            manifest_files.append({
                "offset": offset,
                "path": destination_path,
                "size": destination.stat().st_size,
                "sha256": sha256_file(destination),
            })
            flash_pairs.extend([offset, destination_path])

        full_command = [*base_command, *flash_pairs]
        manifest = {
            "package_format": 2,
            "project": project,
            "project_path": project_path,
            "project_version": description.get("project_version", "unknown"),
            "target": target,
            "idf_version": idf_version,
            "git_sha": git_sha,
            "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "baud": args.baud,
            "flash_settings": flasher.get("flash_settings", {}),
            "extra_esptool_args": extra,
            "files": manifest_files,
            "flash_command": full_command,
        }
        (package_dir / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

        runner_path = package_dir / "flash.py"
        runner_path.write_text(FLASH_RUNNER, encoding="utf-8", newline="\n")
        runner_path.chmod(0o755)

        shell_script = '#!/usr/bin/env sh\nset -eu\ncd "$(dirname "$0")"\nexec python3 flash.py\n'
        shell_path = package_dir / "flash.sh"
        with shell_path.open("w", encoding="utf-8", newline="\n") as output:
            output.write(shell_script)
        shell_path.chmod(0o755)

        cmd_script = '@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\npython flash.py\r\n'
        cmd_path = package_dir / "flash.cmd"
        with cmd_path.open("w", encoding="utf-8", newline="") as output:
            output.write(cmd_script)

        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for item in sorted(package_dir.rglob("*")):
                if item.is_file():
                    relative = item.relative_to(package_dir)
                    archive_path_in_zip = PurePosixPath(package_name, *relative.parts)
                    add_zip_file(archive, item, archive_path_in_zip)

    print(archive_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
