#!/usr/bin/env python3
"""Safely retrieve and flash one exact-HEAD CI firmware package.

The command is deliberately conservative: it discovers this checkout's artifact
contract from its workflows, accepts only completed successful runs for the
current SHA, and never erases flash or advances to another item.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any, Iterable, Sequence


ROOT = Path(__file__).resolve().parents[1]
SCRIPT_DIR = ROOT / "scripts"
WORKFLOW_DIR = ROOT / ".github" / "workflows"
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
UNSAFE_WORDS = ("erase", "read_flash", "write_reg", "run", "execute", "reset")


class CiFirmwareError(RuntimeError):
    """A user-facing safety or contract error."""


@dataclass(frozen=True)
class Repository:
    root: Path
    owner: str
    name: str
    branch: str
    head: str
    clean: bool


@dataclass(frozen=True)
class Item:
    index: int
    workflow: str
    artifact: str
    framework: str
    version: str
    source_project: str
    profile: str


@dataclass(frozen=True)
class RunSelection:
    workflow: str
    run_id: int
    url: str
    sha: str
    artifacts: tuple[dict[str, Any], ...]


def run_checked(args: Sequence[str], cwd: Path = ROOT) -> str:
    try:
        completed = subprocess.run(args, cwd=cwd, check=True, capture_output=True, text=True, encoding="utf-8")
    except (OSError, subprocess.CalledProcessError) as exc:
        detail = getattr(exc, "stderr", "") or getattr(exc, "stdout", "") or str(exc)
        raise CiFirmwareError(f"Command failed ({' '.join(args[:2])}): {detail.strip()}") from exc
    return completed.stdout.strip()


def parse_github_origin(remote: str) -> tuple[str, str]:
    value = remote.strip()
    match = re.fullmatch(r"https://github\.com/([^/\s]+)/([^/\s]+?)(?:\.git)?/?", value)
    if not match:
        match = re.fullmatch(r"git@github\.com:([^/\s]+)/([^/\s]+?)(?:\.git)?", value)
    if not match:
        match = re.fullmatch(r"ssh://git@github\.com/([^/\s]+)/([^/\s]+?)(?:\.git)?/?", value)
    if not match:
        raise CiFirmwareError("origin must be a GitHub HTTPS or SSH repository URL")
    return match.group(1), match.group(2)


def repository(root: Path = ROOT, require_clean: bool = True) -> Repository:
    root = root.resolve()
    owner, name = parse_github_origin(run_checked(["git", "remote", "get-url", "origin"], root))
    branch = run_checked(["git", "symbolic-ref", "--quiet", "--short", "HEAD"], root)
    head = run_checked(["git", "rev-parse", "HEAD"], root).lower()
    if not SHA_RE.fullmatch(head):
        raise CiFirmwareError("local HEAD is not a complete 40-character SHA")
    clean = not bool(run_checked(["git", "status", "--porcelain=v1", "--untracked-files=all"], root))
    if require_clean and not clean:
        raise CiFirmwareError("refusing to flash from a dirty working tree")
    return Repository(root, owner, name, branch, head, clean)


def assert_repository_unchanged(initial: Repository) -> None:
    """Refuse if origin, branch, SHA, or cleanliness changed during the flow."""
    current = repository(initial.root, require_clean=True)
    if current != initial:
        raise CiFirmwareError("repository changed during CI firmware operation; refusing to continue")


def _load_module(name: str, path: Path) -> Any:
    spec = importlib.util.spec_from_file_location(name, path)
    if not spec or not spec.loader:
        raise CiFirmwareError(f"unable to import repository contract: {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def _workflow_text(name: str, root: Path) -> str:
    try:
        return (root / ".github" / "workflows" / name).read_text(encoding="utf-8")
    except OSError as exc:
        raise CiFirmwareError(f"missing workflow contract: {name}") from exc


def expected_items(root: Path = ROOT) -> tuple[Item, ...]:
    """Derive all package names from selector, packager, and checked-in YAML."""
    selector = _load_module("ci_firmware_selector", root / "scripts" / "select_ci_targets.py")
    packager = _load_module("ci_firmware_packager", root / "scripts" / "package_ci_firmware.py")
    inventory = selector.discover_inventory(root)
    idf = _workflow_text("esp-idf.yml", root)
    arduino = _workflow_text("arduino.yml", root)
    firmware = _workflow_text("firmware.yml", root)
    required_idf = "firmware-esp-idf-${{ matrix.name }}-${{ matrix.idf_version }}-rev1_3"
    required_arduino = "firmware-arduino-${{ matrix.name }}-3.3.11-rev1_3"
    required_brookesia = "firmware-brookesia-v5.5.5-${{ matrix.profile }}"
    if required_idf not in idf or required_arduino not in arduino or required_brookesia not in firmware:
        raise CiFirmwareError("workflow artifact templates drifted; refusing to infer packages")
    version_match = re.search(r"ARDUINO_CORE_VERSION:\s*\"([^\"]+)\"", arduino)
    brookesia_version = re.search(r"--framework-version\s+(v[^\s\\]+)", firmware)
    profiles = tuple(re.findall(r"^\s*- profile:\s*([a-z0-9_]+)\s*$", firmware, re.MULTILINE))
    if not version_match or not brookesia_version or profiles != tuple(packager.BOARD_PROFILES):
        raise CiFirmwareError("workflow matrix/profile contract drifted; refusing to infer packages")
    if packager.BOARD != root.name:
        raise CiFirmwareError("packager board does not match this repository basename")
    items: list[Item] = []
    for project in inventory.idf_projects:
        for version in selector.IDF_VERSIONS:
            items.append(Item(0, "esp-idf.yml", f"firmware-esp-idf-{Path(project).name}-{version}-rev1_3", "esp-idf", version, project, "rev1_3"))
    for sketch in inventory.arduino_sketches:
        items.append(Item(0, "arduino.yml", f"firmware-arduino-{Path(sketch).name}-{version_match.group(1)}-rev1_3", "arduino-esp32", version_match.group(1), sketch, "rev1_3"))
    for profile in profiles:
        items.append(Item(0, "firmware.yml", f"firmware-brookesia-{brookesia_version.group(1)}-{profile}", "esp-idf", brookesia_version.group(1), "firmware/brookesia", profile))
    return tuple(Item(index + 1, item.workflow, item.artifact, item.framework, item.version, item.source_project, item.profile) for index, item in enumerate(items))


class GitHubApi:
    def __init__(self, token: str | None = None, force_rest: bool = False) -> None:
        self.token = token or os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
        self.gh = None if force_rest else shutil.which("gh")
        if self.gh:
            probe = subprocess.run([self.gh, "auth", "status"], capture_output=True, text=True, encoding="utf-8")
            if probe.returncode:
                self.gh = None
        if not self.gh and not self.token:
            raise CiFirmwareError("GitHub authentication is required: sign in with gh or set GH_TOKEN/GITHUB_TOKEN")
        if not self.gh:
            self.get_json("/user")

    def get_json(self, endpoint: str) -> Any:
        if self.gh:
            output = run_checked([self.gh, "api", endpoint])
            return json.loads(output)
        request = urllib.request.Request("https://api.github.com" + endpoint, headers={"Accept": "application/vnd.github+json", "Authorization": f"Bearer {self.token}", "User-Agent": "ci-firmware"})
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                return json.loads(response.read().decode("utf-8"))
        except (urllib.error.URLError, urllib.error.HTTPError, json.JSONDecodeError) as exc:
            raise CiFirmwareError(f"GitHub API request failed: {endpoint}") from exc

    def get_all(self, endpoint: str) -> list[dict[str, Any]]:
        result: list[dict[str, Any]] = []
        page = 1
        while True:
            separator = "&" if "?" in endpoint else "?"
            data = self.get_json(f"{endpoint}{separator}per_page=100&page={page}")
            values = data.get("artifacts", data.get("workflow_runs", data if isinstance(data, list) else []))
            if not isinstance(values, list):
                raise CiFirmwareError("GitHub API returned an invalid paginated payload")
            result.extend(value for value in values if isinstance(value, dict))
            if len(values) < 100:
                return result
            page += 1

    def download(self, url: str) -> bytes:
        if self.gh:
            try:
                return subprocess.run([self.gh, "api", url], check=True, capture_output=True).stdout
            except (OSError, subprocess.CalledProcessError) as exc:
                raise CiFirmwareError("artifact archive download failed") from exc
        request = urllib.request.Request(url if url.startswith("https://") else "https://api.github.com" + url, headers={"Accept": "application/vnd.github+json", "Authorization": f"Bearer {self.token}", "User-Agent": "ci-firmware"})
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                return response.read()
        except (urllib.error.URLError, urllib.error.HTTPError) as exc:
            raise CiFirmwareError("artifact archive download failed") from exc


def select_runs(api: Any, repo: Repository, items: Iterable[Item]) -> dict[str, RunSelection]:
    grouped: dict[str, set[str]] = {}
    for item in items:
        grouped.setdefault(item.workflow, set()).add(item.artifact)
    selected: dict[str, RunSelection] = {}
    for workflow, expected in grouped.items():
        runs = api.get_all(f"/repos/{repo.owner}/{repo.name}/actions/workflows/{urllib.parse.quote(workflow)}/runs?branch={urllib.parse.quote(repo.branch)}&status=success")
        candidates = [
            run for run in runs
            if run.get("conclusion") == "success" and run.get("status") == "completed"
            and str(run.get("head_sha", "")).lower() == repo.head
        ]
        if not candidates:
            raise CiFirmwareError(f"no successful completed {workflow} run exists for exact local HEAD {repo.head}")
        latest = max(
            candidates,
            key=lambda run: (
                str(run.get("created_at") or run.get("run_started_at") or ""),
                int(run.get("id", 0) or 0),
            ),
        )
        run_id = int(latest.get("id", 0) or 0)
        if run_id <= 0:
            raise CiFirmwareError(f"latest exact-HEAD {workflow} run has no valid run ID")
        artifacts = tuple(api.get_all(f"/repos/{repo.owner}/{repo.name}/actions/runs/{run_id}/artifacts"))
        matching = [artifact for artifact in artifacts if str(artifact.get("name", "")) in expected]
        names = [str(artifact.get("name", "")) for artifact in matching]
        if len(names) != len(expected) or set(names) != expected or len(set(names)) != len(names):
            raise CiFirmwareError(f"latest exact-HEAD {workflow} run has missing or duplicate expected artifacts; refusing fallback")
        if any(artifact.get("expired") or int(artifact.get("size_in_bytes", 0) or 0) <= 0 for artifact in matching):
            raise CiFirmwareError(f"latest exact-HEAD {workflow} run has expired or empty expected artifacts; refusing fallback")
        selected[workflow] = RunSelection(workflow, run_id, str(latest.get("html_url", "")), repo.head, artifacts)
    return selected


def artifact_for(run: RunSelection, item: Item) -> dict[str, Any]:
    matches = [artifact for artifact in run.artifacts if artifact.get("name") == item.artifact]
    if len(matches) != 1:
        raise CiFirmwareError(f"selected run does not contain exactly one {item.artifact} artifact")
    artifact = matches[0]
    if artifact.get("expired") or int(artifact.get("size_in_bytes", 0) or 0) <= 0:
        raise CiFirmwareError(f"artifact metadata is expired or has no content: {item.artifact}")
    return artifact


def safe_extract(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=False)
    destination_root = destination.resolve()
    with zipfile.ZipFile(archive) as bundle:
        for member in bundle.infolist():
            raw_name = member.filename
            name = PurePosixPath(member.filename)
            is_link = (member.external_attr >> 16) & 0o170000 == 0o120000
            if (not raw_name or "\\" in raw_name or raw_name.startswith("//")
                    or re.match(r"^[A-Za-z]:", raw_name) or name.is_absolute()
                    or ".." in name.parts or is_link):
                raise CiFirmwareError("artifact ZIP contains an unsafe path or symlink")
            target = destination.joinpath(*name.parts)
            try:
                target.resolve().relative_to(destination_root)
            except ValueError as exc:
                raise CiFirmwareError("artifact ZIP target escapes extraction directory") from exc
            if target.exists():
                raise CiFirmwareError("artifact ZIP would overwrite an extracted file")
            if member.is_dir():
                target.mkdir(parents=True, exist_ok=False)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with bundle.open(member) as source, target.open("xb") as output:
                    shutil.copyfileobj(source, output)


def unpack_artifact(payload: bytes, cache: Path) -> Path:
    outer = cache / "artifact.zip"
    outer.write_bytes(payload)
    outer_dir = cache / "outer"
    safe_extract(outer, outer_dir)
    packages = list(outer_dir.rglob("*.zip"))
    if len(packages) != 1:
        raise CiFirmwareError("Actions artifact must contain exactly one CI package ZIP")
    package_dir = cache / "package"
    safe_extract(packages[0], package_dir)
    manifests = list(package_dir.rglob("manifest.json"))
    if len(manifests) != 1:
        raise CiFirmwareError("CI package must contain exactly one manifest.json")
    return manifests[0].parent


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_offset(value: Any) -> int:
    if not isinstance(value, str):
        raise CiFirmwareError("flash offset must be a string")
    try:
        parsed = int(value, 0)
    except (TypeError, ValueError) as exc:
        raise CiFirmwareError(f"invalid flash offset: {value!r}") from exc
    if parsed < 0:
        raise CiFirmwareError("flash offsets must not be negative")
    return parsed


def _no_duplicate_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise CiFirmwareError(f"JSON contains duplicate key: {key}")
        result[key] = value
    return result


def load_json_object(path: Path) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_no_duplicate_object)
    except (OSError, json.JSONDecodeError) as exc:
        raise CiFirmwareError(f"invalid JSON document: {path.name}") from exc
    if not isinstance(document, dict):
        raise CiFirmwareError(f"JSON document must be an object: {path.name}")
    return document


def _string(value: Any, description: str) -> str:
    if not isinstance(value, str):
        raise CiFirmwareError(f"{description} must be a string")
    return value


def _integer(value: Any, description: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise CiFirmwareError(f"{description} must be an integer")
    return value


def _safe_relative(value: str, description: str) -> PurePosixPath:
    if "\\" in value or value.startswith("//") or re.match(r"^[A-Za-z]:", value):
        raise CiFirmwareError(f"{description} is unsafe")
    relative = PurePosixPath(value)
    if not relative.parts or relative.is_absolute() or ".." in relative.parts:
        raise CiFirmwareError(f"{description} is unsafe")
    return relative


def validate_idf_metadata(package: Path, plan: list[tuple[int, int, Path, str]], raw_files: list[Any]) -> None:
    metadata = load_json_object(package / "metadata" / "flasher_args.json")
    flash_files = metadata.get("flash_files")
    if not isinstance(flash_files, dict) or not flash_files:
        raise CiFirmwareError("ESP-IDF metadata has no flash_files object")
    expected: set[tuple[int, str]] = set()
    for raw_offset, raw_path in flash_files.items():
        if not isinstance(raw_offset, str) or not isinstance(raw_path, str):
            raise CiFirmwareError("ESP-IDF metadata flash_files must map strings to strings")
        expected.add((parse_offset(raw_offset), _safe_relative(raw_path, "ESP-IDF metadata path").as_posix()))
    actual: set[tuple[int, str]] = set()
    for record in raw_files:
        metadata_path = _string(record.get("metadata_path"), "manifest metadata_path")
        actual.add((parse_offset(record.get("offset")), _safe_relative(metadata_path, "manifest metadata_path").as_posix()))
    if len(actual) != len(plan) or actual != expected:
        raise CiFirmwareError("manifest verified files do not exactly match ESP-IDF flasher metadata")


def validate_arduino_layout(
    package: Path,
    item: Item,
    manifest: dict[str, Any],
    plan: list[tuple[int, int, Path, str]],
    raw_files: list[Any],
    packager: Any,
) -> list[str]:
    if (item.profile != "rev1_3" or manifest.get("fqbn") != packager.ARDUINO_FQBN
            or manifest.get("target") != packager.CHIP):
        raise CiFirmwareError("Arduino manifest FQBN or profile does not match the packager contract")
    build_identity = manifest.get("arduino_build_identity")
    if (not isinstance(build_identity, dict)
            or build_identity.get("path") != "metadata/arduino_build_identity.json"
            or build_identity.get("raw_archived") is not False):
        raise CiFirmwareError("Arduino manifest lacks the privacy-safe build.options.json FQBN proof")
    canonical_path = package / "metadata" / "arduino_build_identity.json"
    canonical_size = _integer(build_identity.get("size"), "Arduino build identity size")
    canonical_hash = _string(build_identity.get("sha256"), "Arduino build identity SHA-256").lower()
    if (canonical_size <= 0 or not re.fullmatch(r"[0-9a-f]{64}", canonical_hash)
            or not canonical_path.is_file() or canonical_path.stat().st_size != canonical_size
            or sha256(canonical_path) != canonical_hash):
        raise CiFirmwareError("Arduino canonical build identity size or hash verification failed")
    canonical = load_json_object(canonical_path)
    expected_canonical_keys = {
        "schema_version", "source_name", "source_size", "source_sha256", "fqbn", "core_version",
        "sketch_path", "sketch_name", "application_metadata_path", "primary_source",
    }
    if set(canonical) != expected_canonical_keys:
        raise CiFirmwareError("Arduino canonical build identity contains unknown or missing fields")
    primary_source = canonical.get("primary_source")
    primary_sha = _string(primary_source.get("sha256"), "primary source SHA-256") if isinstance(primary_source, dict) else ""
    primary_blob = _string(primary_source.get("git_blob_sha"), "primary source Git blob SHA") if isinstance(primary_source, dict) else ""
    if (
        not isinstance(primary_source, dict)
        or set(primary_source) != {"path", "size", "sha256", "git_blob_sha"}
        or primary_source.get("path") != f"{item.source_project}/{Path(item.source_project).name}.ino"
        or not isinstance(primary_source.get("size"), int)
        or isinstance(primary_source.get("size"), bool)
        or primary_source.get("size") <= 0
        or primary_sha != primary_sha.lower()
        or not re.fullmatch(r"[0-9a-f]{64}", primary_sha)
        or primary_blob != primary_blob.lower()
        or not re.fullmatch(r"[0-9a-f]{40}", primary_blob)
    ):
        raise CiFirmwareError("Arduino canonical primary-source identity is malformed or does not match the sketch")
    source_size = _integer(canonical.get("source_size"), "raw build.options size")
    source_hash = _string(canonical.get("source_sha256"), "raw build.options SHA-256").lower()
    if (canonical.get("schema_version") != 1 or canonical.get("source_name") != "build.options.json"
            or source_size <= 0 or not re.fullmatch(r"[0-9a-f]{64}", source_hash)
            or canonical.get("fqbn") != packager.ARDUINO_FQBN
            or canonical.get("core_version") != packager.ARDUINO_CORE_VERSION
            or canonical.get("sketch_path") != item.source_project
            or canonical.get("sketch_name") != Path(item.source_project).name):
        raise CiFirmwareError("Arduino canonical build identity differs from the package contract")
    try:
        canonical_application_path = _safe_relative(
            _string(
                canonical.get("application_metadata_path"),
                "Arduino canonical application metadata path",
            ),
            "Arduino canonical application metadata path",
        ).as_posix()
    except CiFirmwareError:
        raise
    if not packager._portable_helper_archive_path(canonical_application_path):
        raise CiFirmwareError("Arduino canonical application metadata path is not portable")
    manifest_source_name = _string(build_identity.get("source_name"), "manifest raw build source name")
    manifest_source_size = _integer(build_identity.get("source_size"), "manifest raw build source size")
    manifest_source_hash = _string(build_identity.get("source_sha256"), "manifest raw build source SHA-256").lower()
    if (manifest_source_name != canonical["source_name"]
            or manifest_source_size != canonical["source_size"]
            or manifest_source_hash != canonical["source_sha256"]):
        raise CiFirmwareError("Arduino manifest raw build identity evidence differs from canonical metadata")
    metadata_contract = manifest.get("arduino_metadata")
    if not isinstance(metadata_contract, dict) or metadata_contract.get("path") != "metadata/flash_args":
        raise CiFirmwareError("Arduino manifest must identify its packaged flash_args metadata")
    metadata_path = package / "metadata" / "flash_args"
    metadata_size = _integer(metadata_contract.get("size"), "Arduino metadata size")
    metadata_hash = _string(metadata_contract.get("sha256"), "Arduino metadata SHA-256").lower()
    if (metadata_size <= 0 or not re.fullmatch(r"[0-9a-f]{64}", metadata_hash)
            or not metadata_path.is_file() or metadata_path.stat().st_size != metadata_size
            or sha256(metadata_path) != metadata_hash):
        raise CiFirmwareError("Arduino flash_args metadata size or hash verification failed")
    try:
        metadata_options, metadata_entries = packager.parse_arduino_flash_args_text(
            metadata_path.read_text(encoding="utf-8")
        )
        write_flash_args = packager.validate_arduino_write_flash_args(
            manifest.get("arduino_write_flash_args")
        )
    except (OSError, UnicodeError, ValueError) as exc:
        raise CiFirmwareError(f"Arduino flash_args metadata is invalid: {exc}") from exc
    if write_flash_args != metadata_options:
        raise CiFirmwareError("Arduino manifest write options differ from packaged flash_args")

    bsp = manifest.get("bsp")
    if (not isinstance(bsp, dict) or bsp.get("component") != packager.ARDUINO_BSP_COMPONENT
            or bsp.get("applicable") is not False):
        raise CiFirmwareError("Arduino BSP evidence binding is missing or falsely marked applicable")
    try:
        bsp_version, bsp_source_git_sha, bsp_component_tree_sha = packager.validate_bsp_binding(
            _string(bsp.get("version"), "Arduino BSP version"),
            _string(bsp.get("source_git_sha"), "Arduino BSP source Git SHA"),
            _string(bsp.get("component_git_tree_sha"), "Arduino BSP component Git tree SHA"),
        )
    except ValueError as exc:
        raise CiFirmwareError(str(exc)) from exc

    actual_entries: list[tuple[int, str]] = []
    roles: list[str] = []
    seen_archive_paths: set[str] = set()
    for record in raw_files:
        if not isinstance(record, dict):
            raise CiFirmwareError("Arduino manifest contains an invalid segment record")
        metadata_relative = _safe_relative(
            _string(record.get("metadata_path"), "Arduino metadata path"),
            "Arduino metadata path",
        ).as_posix()
        archive_relative = _safe_relative(
            _string(record.get("archive_path"), "Arduino archive path"),
            "Arduino archive path",
        ).as_posix()
        if (not packager._portable_helper_archive_path(metadata_relative)
                or not packager._portable_helper_archive_path(archive_relative)
                or not archive_relative.startswith("bin/")
                or archive_relative in seen_archive_paths
                or packager._arduino_whole_flash_name(archive_relative)):
            raise CiFirmwareError("Arduino manifest contains a duplicate or whole-flash archive path")
        seen_archive_paths.add(archive_relative)
        raw_offset = parse_offset(record.get("offset"))
        actual_entries.append((raw_offset, metadata_relative))
        try:
            expected_role = packager.arduino_segment_role(
                metadata_relative, Path(item.source_project).name
            )
        except ValueError as exc:
            raise CiFirmwareError(str(exc)) from exc
        role = _string(record.get("role"), "Arduino segment role")
        roles.append(role)
        per_segment = {
            "metadata_source": "metadata/flash_args",
            "role": expected_role,
            "target": packager.CHIP,
            "fqbn": packager.ARDUINO_FQBN,
            "framework_version": item.version,
            "product_git_sha": manifest.get("git_sha"),
            "bsp_component": packager.ARDUINO_BSP_COMPONENT,
            "bsp_version": bsp_version,
            "bsp_source_git_sha": bsp_source_git_sha,
            "bsp_component_git_tree_sha": bsp_component_tree_sha,
            "bsp_applicable": False,
        }
        if any(record.get(key) != value for key, value in per_segment.items()):
            raise CiFirmwareError("Arduino segment provenance differs from the package identity")
    if actual_entries != metadata_entries:
        raise CiFirmwareError("Arduino manifest segments do not exactly match packaged flash_args")
    if any(roles.count(required) != 1 for required in ("bootloader", "partition_table", "application")):
        raise CiFirmwareError("Arduino metadata must contain one bootloader, partition table, and application")
    if roles.count("boot_app0") > 1:
        raise CiFirmwareError("Arduino metadata contains duplicate boot_app0 segments")
    application_metadata_paths = [
        metadata_path
        for (_, metadata_path), role in zip(actual_entries, roles)
        if role == "application"
    ]
    if application_metadata_paths != [canonical_application_path]:
        raise CiFirmwareError("Arduino application output differs from its canonical sketch identity")

    flash = manifest.get("flash")
    assert isinstance(flash, dict)
    segmented_bytes = sum(size for _, size, _, _ in plan)
    fraction = round(segmented_bytes / packager.FLASH_SIZE, 8)
    if (_integer(flash.get("full_flash_bytes"), "Arduino full flash bytes") != packager.FLASH_SIZE
            or _integer(flash.get("segmented_bytes"), "Arduino segmented bytes") != segmented_bytes
            or _integer(flash.get("segmented_payload_total"), "Arduino segmented payload total") != segmented_bytes
            or flash.get("segmented_fraction") != fraction
            or segmented_bytes > packager.FLASH_SIZE // 2):
        raise CiFirmwareError("Arduino segmented-byte evidence is missing or not smaller than full flash")

    expected_inventory = {
        "manifest.json", "flash.sh", "flash.cmd", "metadata/flash_args",
        "metadata/arduino_build_identity.json",
        *seen_archive_paths,
    }
    actual_inventory: set[str] = set()
    for path in package.rglob("*"):
        if path.is_symlink():
            raise CiFirmwareError("Arduino package contains a symlink")
        if path.is_file():
            relative = path.relative_to(package).as_posix()
            actual_inventory.add(relative)
            if "build.options" in relative.casefold() or "expanded" in relative.casefold():
                raise CiFirmwareError("Arduino package contains raw build properties")
            if packager.is_binary_payload(relative):
                continue
            try:
                packager.reject_private_artifact_bytes(path.read_bytes(), f"Arduino package member {relative}")
            except ValueError as exc:
                raise CiFirmwareError(str(exc)) from exc
    if actual_inventory != expected_inventory:
        raise CiFirmwareError("Arduino package extracted file inventory is not exact")
    _, expected_shell, expected_batch = packager.flash_helpers(raw_files, write_flash_args)
    try:
        if ((package / "flash.sh").read_bytes() != expected_shell.encode("utf-8")
                or (package / "flash.cmd").read_bytes() != expected_batch.encode("utf-8")):
            raise CiFirmwareError("Arduino flash helpers differ from the verified metadata plan")
    except OSError as exc:
        raise CiFirmwareError("Arduino flash helpers are missing or unreadable") from exc
    return write_flash_args


def validate_manifest(package: Path, item: Item, repo: Repository) -> tuple[list[tuple[int, Path]], list[str]]:
    packager = _load_module("ci_firmware_manifest_packager", repo.root / "scripts" / "package_ci_firmware.py")
    try:
        trusted_root, trusted_head = packager.trusted_repository()
    except ValueError as exc:
        raise CiFirmwareError(f"trusted repository verification failed: {exc}") from exc
    if (trusted_root != repo.root.resolve()
            or trusted_head != repo.head
            or repo.clean is not True):
        raise CiFirmwareError("selected repository does not match its clean script-derived HEAD")
    manifest = load_json_object(package / "manifest.json")
    if item.framework == "arduino-esp32":
        try:
            strict_manifest = packager.validate_arduino_extracted_bundle(package, trusted_head)
        except (OSError, UnicodeError, ValueError) as exc:
            raise CiFirmwareError(f"strict Arduino package validation failed: {exc}") from exc
        if strict_manifest != manifest:
            raise CiFirmwareError("Arduino manifest changed between independent validation passes")
    identity = {"schema_version": 1, "board": repo.name, "chip": packager.CHIP, "board_profile": item.profile, "framework": item.framework, "framework_version": item.version, "source_project": item.source_project, "git_sha": repo.head, "c6_firmware_included": False}
    for key in ("board", "chip", "board_profile", "framework", "framework_version", "source_project", "git_sha"):
        _string(manifest.get(key), f"manifest {key}")
    if not isinstance(manifest.get("c6_firmware_included"), bool):
        raise CiFirmwareError("manifest c6_firmware_included must be a boolean")
    if (packager.BOARD != repo.name or not isinstance(manifest.get("schema_version"), int)
            or isinstance(manifest.get("schema_version"), bool)
            or any(manifest.get(key) != value for key, value in identity.items())):
        raise CiFirmwareError("manifest identity does not match the selected exact-HEAD item")
    flash = manifest.get("flash")
    if (not isinstance(flash, dict) or _integer(flash.get("size_bytes"), "flash size") != packager.FLASH_SIZE
            or _integer(flash.get("baud"), "flash baud") != packager.DEFAULT_BAUD):
        raise CiFirmwareError("manifest flash capacity or baud does not match the packager contract")
    command = _string(flash.get("command"), "flash command")
    if any(word in command.casefold() for word in UNSAFE_WORDS):
        raise CiFirmwareError("manifest flash command is unsafe")
    tokens = shlex.split(command)
    base_prefix = ["python", "-m", "esptool", "--chip", packager.CHIP, "--baud", str(packager.DEFAULT_BAUD), "write_flash"]
    write_flash_args: list[str] = []
    if item.framework == "arduino-esp32":
        try:
            write_flash_args = packager.validate_arduino_write_flash_args(
                manifest.get("arduino_write_flash_args")
            )
        except ValueError as exc:
            raise CiFirmwareError(str(exc)) from exc
    prefix = base_prefix + write_flash_args
    if tokens[:len(prefix)] != prefix or len(tokens) < len(prefix) + 2 or (len(tokens) - len(prefix)) % 2:
        raise CiFirmwareError("manifest flash command is not a canonical write_flash plan")
    raw_files = manifest.get("files")
    if not isinstance(raw_files, list) or not raw_files:
        raise CiFirmwareError("manifest contains no flash files")
    plan: list[tuple[int, int, Path, str]] = []
    for record in raw_files:
        if not isinstance(record, dict):
            raise CiFirmwareError("manifest file entry is invalid")
        relative = _safe_relative(_string(record.get("archive_path"), "manifest archive path"), "manifest archive path")
        path = package.joinpath(*relative.parts)
        try:
            path.resolve().relative_to(package.resolve())
        except ValueError as exc:
            raise CiFirmwareError("manifest archive path escapes package") from exc
        size, offset = _integer(record.get("size"), "manifest file size"), parse_offset(record.get("offset"))
        expected_hash = _string(record.get("sha256"), "manifest SHA-256").lower()
        if size <= 0 or not re.fullmatch(r"[0-9a-f]{64}", expected_hash) or not path.is_file() or path.stat().st_size != size or sha256(path) != expected_hash:
            raise CiFirmwareError("manifest file hash or size verification failed")
        plan.append((offset, size, path, relative.as_posix()))
    plan.sort(key=lambda entry: entry[0])
    previous_end = -1
    for offset, size, _, _ in plan:
        if offset < previous_end or offset + size > packager.FLASH_SIZE:
            raise CiFirmwareError("manifest flash ranges overlap or exceed capacity")
        previous_end = offset + size
    pairs = [(parse_offset(tokens[index]), tokens[index + 1]) for index in range(len(prefix), len(tokens), 2)]
    expected_pairs = [(offset, relative) for offset, _, _, relative in plan]
    if pairs != expected_pairs:
        raise CiFirmwareError("manifest command does not exactly match sorted verified offsets and paths")
    profile = manifest.get("chip_revision")
    expected_profile = packager.BOARD_PROFILES[item.profile]
    if (not isinstance(profile, dict) or _string(profile.get("minimum"), "manifest chip minimum") != expected_profile["minimum"]
            or profile.get("maximum_exclusive") != expected_profile["maximum_exclusive"]
            or (profile.get("maximum_exclusive") is not None and not isinstance(profile.get("maximum_exclusive"), str))):
        raise CiFirmwareError("manifest chip revision range does not match its profile")
    if item.framework == "esp-idf":
        validate_idf_metadata(package, plan, raw_files)
    elif item.framework == "arduino-esp32":
        if validate_arduino_layout(package, item, manifest, plan, raw_files, packager) != write_flash_args:
            raise CiFirmwareError("Arduino write options changed during metadata validation")
    else:
        raise CiFirmwareError("unsupported package framework")
    return [(offset, path) for offset, _, path, _ in plan], write_flash_args


def profile_for_major(major: int) -> str:
    return "rev1_3" if major < 3 else "rev3_x"


def parse_probe(output: str) -> int:
    if "esp32-p4" not in output.casefold():
        raise CiFirmwareError("serial probe did not prove an ESP32-P4 target")
    match = re.search(r"(?:revision|rev)\D*([0-9]+)", output, re.IGNORECASE)
    if not match:
        raise CiFirmwareError("serial probe did not provide an ESP32-P4 major revision")
    return int(match.group(1))


def parse_flash_size(output: str) -> int:
    match = re.search(r"(?:detected\s+flash\s+size|flash\s+size)\s*:\s*([0-9]+)\s*(Mbit|MiB|MB)\b", output, re.IGNORECASE)
    if not match:
        raise CiFirmwareError("flash_id did not provide a parsable flash size")
    count, unit = int(match.group(1)), match.group(2).casefold()
    return count * 1024 * 1024 if unit in {"mb", "mib"} else count * 1024 * 1024 // 8


def list_ports() -> list[str]:
    ports: set[str] = set()
    try:
        from serial.tools import list_ports as serial_ports
        ports.update(port.device for port in serial_ports.comports())
    except ImportError:
        pass
    if os.name != "nt":
        for pattern in ("/dev/ttyUSB*", "/dev/ttyACM*", "/dev/serial/by-id/*"):
            ports.update(str(path) for path in Path("/").glob(pattern.lstrip("/")))
    return sorted(ports)


def esptool_output(port: str, command: str) -> str:
    alternatives = (command, command.replace("_", "-"))
    output = ""
    for alternative in alternatives:
        completed = subprocess.run([sys.executable, "-m", "esptool", "--chip", "esp32p4", "--port", port, alternative], capture_output=True, text=True, encoding="utf-8")
        output = completed.stdout + completed.stderr
        if completed.returncode == 0:
            return output
    raise CiFirmwareError(f"esptool {command} probe failed: {output.strip()}")


def cache_dir(repo: Repository, item: Item) -> Path:
    base = Path(os.environ.get("LOCALAPPDATA", "")) if os.name == "nt" and os.environ.get("LOCALAPPDATA") else Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    return base / "waveshare-ci-firmware" / repo.owner / repo.name / repo.head / item.artifact


def resolve_item(items: Sequence[Item], value: str) -> Item:
    matches = [item for item in items if str(item.index) == value or item.artifact == value]
    if len(matches) != 1:
        raise CiFirmwareError("--item must be one displayed index or exact artifact name")
    return matches[0]


def print_items(items: Sequence[Item], runs: dict[str, RunSelection]) -> None:
    for item in items:
        run = runs[item.workflow]
        print(f"{item.index}: artifact={item.artifact} profile={item.profile} workflow={item.workflow} run={run.run_id} sha={run.sha}")


def preflight(repo: Repository, api: Any, items: Sequence[Item], need_esptool: bool = True) -> dict[str, RunSelection]:
    if not repo.clean:
        raise CiFirmwareError("refusing to preflight from a dirty working tree")
    print(f"origin={repo.owner}/{repo.name} branch={repo.branch} HEAD={repo.head} clean={str(repo.clean).lower()}")
    if need_esptool:
        try:
            __import__("esptool")
        except ImportError as exc:
            raise CiFirmwareError("Python esptool module is required") from exc
    runs = select_runs(api, repo, items)
    for item in items:
        artifact_for(runs[item.workflow], item)
    for run in runs.values():
        print(f"workflow={run.workflow} run={run.run_id} url={run.url} sha={run.sha} artifacts={len(run.artifacts)}")
    return runs


def self_test() -> None:
    assert parse_github_origin("https://github.com/example/repo.git") == ("example", "repo")
    assert parse_github_origin("git@github.com:example/repo.git") == ("example", "repo")
    try:
        parse_github_origin("https://example.invalid/a/b")
    except CiFirmwareError:
        pass
    else:
        raise AssertionError("non-GitHub origin accepted")
    assert len(expected_items(ROOT)) == 33
    assert profile_for_major(2) == "rev1_3" and profile_for_major(3) == "rev3_x"
    assert parse_flash_size("Detected flash size: 32MB") == 32 * 1024 * 1024
    with tempfile.TemporaryDirectory() as temporary:
        source, destination = Path(temporary) / "bad.zip", Path(temporary) / "out"
        with zipfile.ZipFile(source, "w") as archive: archive.writestr("../escape", "x")
        try: safe_extract(source, destination)
        except CiFirmwareError: pass
        else: raise AssertionError("unsafe ZIP accepted")
    print("SELF_TEST_OK items=33 origin=ok safe-zip=ok profiles=ok no-auto-next=ok")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--preflight", action="store_true")
    parser.add_argument("--item")
    parser.add_argument("--port")
    args = parser.parse_args(argv)
    try:
        if args.self_test:
            self_test(); return 0
        repo = repository(ROOT, require_clean=True)
        items = expected_items(ROOT)
        api = GitHubApi()
        runs = preflight(repo, api, items, need_esptool=not args.list)
        if args.preflight:
            print("PREFLIGHT_OK")
            return 0
        if args.list:
            print_items(items, runs)
            return 0
        choice = args.item or input("Select item index or exact artifact name: ").strip()
        item = resolve_item(items, choice)
        artifact = artifact_for(runs[item.workflow], item)
        target_cache = cache_dir(repo, item)
        target_cache.parent.mkdir(parents=True, exist_ok=True)
        if target_cache.exists():
            raise CiFirmwareError(f"refusing to overwrite existing cache directory: {target_cache}")
        assert_repository_unchanged(repo)
        package = unpack_artifact(api.download(str(artifact.get("archive_download_url", ""))), target_cache)
        plan, write_flash_args = validate_manifest(package, item, repo)
        assert_repository_unchanged(repo)
        ports = list_ports()
        print("ports=" + (", ".join(ports) if ports else "none"))
        port = args.port or input("Select serial port: ").strip()
        if not port:
            raise CiFirmwareError("a serial port is required for flashing")
        major = parse_probe(esptool_output(port, "chip_id"))
        capacity = parse_flash_size(esptool_output(port, "flash_id"))
        packager = _load_module("ci_firmware_flash_packager", ROOT / "scripts" / "package_ci_firmware.py")
        if profile_for_major(major) != item.profile:
            raise CiFirmwareError(f"ESP32-P4 major revision {major} requires {profile_for_major(major)}, not {item.profile}")
        if capacity < packager.FLASH_SIZE or capacity < 32 * 1024 * 1024:
            raise CiFirmwareError("detected flash capacity is below the required 32 MiB")
        if item.profile == "rev3_x": print("WARNING: confirm matching rev3_x PCB/electrical revision before flashing.")
        print(f"run={runs[item.workflow].run_id} sha={repo.head} artifact={item.artifact} profile={item.profile} port={port} chip=ESP32-P4 revision={major} flash={capacity}")
        print("plan=" + " ".join([*write_flash_args, *(f"0x{offset:x}:{path}" for offset, path in plan)]))
        if input("Type FLASH to write this one item: ").strip() != "FLASH":
            raise CiFirmwareError("flash confirmation was not provided")
        assert_repository_unchanged(repo)
        command = [sys.executable, "-m", "esptool", "--port", port, "--chip", packager.CHIP, "--baud", str(packager.DEFAULT_BAUD), "write_flash", *write_flash_args] + [part for offset, path in plan for part in (f"0x{offset:x}", str(path))]
        completed = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
        output = completed.stdout + completed.stderr
        print(output, end="")
        if completed.returncode or "Hash of data verified" not in output:
            raise CiFirmwareError("flash did not exit successfully with Hash of data verified")
        print("FLASH_OK")
        return 0
    except CiFirmwareError as exc:
        print(f"CI firmware error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
