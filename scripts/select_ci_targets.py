#!/usr/bin/env python3
"""Select first-party CI targets from a complete Git changed-file scope.

The selector deliberately keeps maintained firmware outside example CI.  It
discovers immediate ESP-IDF projects and recursive first-party Arduino sketches,
routes direct example changes narrowly, and falls back to the complete framework
matrix for unknown non-documentation inputs.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable, Sequence


IDF_VERSIONS = ("v5.5.5", "v6.0.2")
IDF_IMAGES = {
    "v5.5.5": "espressif/idf:v5.5.5@sha256:a9231d0697ab8f7517cc072e93b7c83e04907bfbfba80b6440d7dbbf90665cf2",
    "v6.0.2": "espressif/idf:v6.0.2@sha256:0d8c9773d48a327233f9c1d7c654ff0bcf133ae24503ea2e97a57cfe02b8cb67",
}
DOCUMENT_EXTENSIONS = {
    ".md",
    ".mdx",
    ".rst",
    ".pdf",
    ".dxf",
    ".png",
    ".jpg",
    ".jpeg",
    ".gif",
    ".svg",
    ".webp",
}
TOP_LEVEL_GOVERNANCE = {
    ".editorconfig",
    ".gitignore",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
    "CONTRIBUTING_ZH.md",
    "LICENSE",
    "LICENSE.md",
    "README.md",
    "README_ZH.md",
    "SECURITY.md",
    "SECURITY_ZH.md",
    "SUPPORT.md",
    "SUPPORT_ZH.md",
    "THIRD_PARTY_NOTICES.md",
    "THIRD_PARTY_NOTICES_ZH.md",
    "config/markdown-audit.json",
    "scripts/check_repository_policy.py",
}
GLOBAL_CI_INPUTS = {
    "Flash-CI-Firmware.cmd",
    "Flash-CI-Firmware.sh",
    "scripts/Flash-CI-Firmware.ps1",
    "scripts/ci_firmware.py",
    "scripts/collect_ci_changes.py",
    "scripts/package_ci_firmware.py",
    "scripts/check_repository_policy.py",
    "tests/test_ci_firmware.py",
    "tests/test_collect_ci_changes.py",
    "tests/test_repository_policy.py",
}


class RoutingError(RuntimeError):
    """Raised when changed-file scope or a manual target is invalid."""


@dataclass(frozen=True)
class Inventory:
    idf_projects: tuple[str, ...]
    arduino_sketches: tuple[str, ...]


@dataclass(frozen=True)
class Selection:
    builds: tuple[dict[str, str], ...]
    selected: tuple[str, ...]
    firmware_touched: bool
    changed_paths: tuple[str, ...]


def normalize_path(raw: str) -> str:
    """Return a repository-relative POSIX path without a leading ``./``."""

    value = raw.replace("\\", "/")
    while value.startswith("./"):
        value = value[2:]
    value = value.rstrip("/")
    if (
        not value
        or value.startswith("/")
        or re.match(r"^[A-Za-z]:/", value)
        or ".." in PurePosixPath(value).parts
    ):
        raise RoutingError(f"Invalid repository-relative path: {raw!r}")
    return value


def discover_inventory(root: Path) -> Inventory:
    """Discover immediate ESP-IDF projects and nested first-party sketches."""

    idf_root = root / "examples" / "esp-idf"
    arduino_root = root / "examples" / "arduino"

    idf_projects = tuple(
        path.relative_to(root).as_posix()
        for path in sorted(idf_root.iterdir(), key=lambda item: item.name.casefold())
        if path.is_dir() and (path / "CMakeLists.txt").is_file()
    )

    arduino_sketches: list[str] = []
    for sketch in sorted(arduino_root.rglob("*.ino"), key=lambda item: item.as_posix().casefold()):
        relative = sketch.relative_to(arduino_root)
        if "libraries" in relative.parts or sketch.stem != sketch.parent.name:
            continue
        arduino_sketches.append(sketch.parent.relative_to(root).as_posix())

    if not idf_projects:
        raise RoutingError("No first-party ESP-IDF projects were discovered.")
    if not arduino_sketches:
        raise RoutingError("No first-party Arduino sketches were discovered.")

    return Inventory(idf_projects, tuple(arduino_sketches))


def _decode_field(value: bytes) -> str:
    return value.decode("utf-8", errors="surrogateescape")


def parse_name_status(path: Path) -> tuple[str, ...]:
    """Parse ``git diff --name-status`` output, including ``-z`` output.

    Both sides of copies and renames are retained so deletions and moves cannot
    silently remove an affected project from the routing decision.
    """

    data = path.read_bytes()
    if not data:
        raise RoutingError("Changed-file input is empty; refusing a false green run.")

    changed: list[str] = []
    if b"\0" in data:
        fields = [_decode_field(item) for item in data.split(b"\0") if item]
        index = 0
        while index < len(fields):
            field = fields[index]
            if "\t" in field:
                status, first_path = field.split("\t", 1)
                index += 1
            else:
                status = field
                index += 1
                if index >= len(fields):
                    raise RoutingError(f"Missing path after Git status {status!r}.")
                first_path = fields[index]
                index += 1

            changed.append(normalize_path(first_path))
            if status[:1] in {"R", "C"}:
                if index >= len(fields):
                    raise RoutingError(f"Missing destination path after Git status {status!r}.")
                changed.append(normalize_path(fields[index]))
                index += 1
    else:
        for raw_line in data.decode("utf-8", errors="surrogateescape").splitlines():
            if not raw_line.strip():
                continue
            fields = raw_line.split("\t")
            status = fields[0]
            expected = 3 if status[:1] in {"R", "C"} else 2
            if len(fields) < expected:
                raise RoutingError(f"Malformed name-status line: {raw_line!r}")
            changed.append(normalize_path(fields[1]))
            if expected == 3:
                changed.append(normalize_path(fields[2]))

    if not changed:
        raise RoutingError("Changed-file input contains no paths.")
    return tuple(dict.fromkeys(changed))


def is_documentation_or_governance(path: str) -> bool:
    posix = PurePosixPath(path)
    if posix.suffix.lower() in DOCUMENT_EXTENSIONS:
        return True
    if path in TOP_LEVEL_GOVERNANCE:
        return True
    if path.startswith(".github/ISSUE_TEMPLATE/"):
        return True
    if path.startswith(".github/PULL_REQUEST_TEMPLATE"):
        return True
    if path in {".github/CODEOWNERS", ".github/dependabot.yml", ".github/dependabot.yaml"}:
        return True
    return False


def _project_for_path(path: str, projects: Sequence[str]) -> str | None:
    for project in projects:
        if path == project or path.startswith(f"{project}/"):
            return project
    return None


def _route_path(path: str, framework: str, inventory: Inventory) -> tuple[str, ...]:
    """Return selected project paths for one changed path and framework."""

    if path in GLOBAL_CI_INPUTS:
        return inventory.idf_projects if framework == "esp-idf" else inventory.arduino_sketches
    if is_documentation_or_governance(path):
        return ()
    if path == "firmware" or path.startswith("firmware/"):
        return ()

    if path == "scripts/select_ci_targets.py" or path.startswith("tests/test_select_ci_targets"):
        return inventory.idf_projects if framework == "esp-idf" else inventory.arduino_sketches

    if path.startswith(".github/workflows/"):
        workflow = PurePosixPath(path).name
        if workflow == "esp-idf.yml":
            return inventory.idf_projects if framework == "esp-idf" else ()
        if workflow == "arduino.yml":
            return inventory.arduino_sketches if framework == "arduino" else ()
        if workflow in {"firmware.yml", "repository-policy.yml"}:
            return ()
        return inventory.idf_projects if framework == "esp-idf" else inventory.arduino_sketches

    idf_project = _project_for_path(path, inventory.idf_projects)
    if idf_project:
        return (idf_project,) if framework == "esp-idf" else ()
    if path.startswith("examples/esp-idf/"):
        return inventory.idf_projects if framework == "esp-idf" else ()

    if path.startswith("examples/arduino/libraries/"):
        return inventory.arduino_sketches if framework == "arduino" else ()
    arduino_sketch = _project_for_path(path, inventory.arduino_sketches)
    if arduino_sketch:
        return (arduino_sketch,) if framework == "arduino" else ()
    if path.startswith("examples/arduino/"):
        return inventory.arduino_sketches if framework == "arduino" else ()

    if path == "config" or path.startswith("config/"):
        return inventory.idf_projects if framework == "esp-idf" else ()
    if path == "scripts/package_esp_idf_firmware.py":
        return inventory.idf_projects if framework == "esp-idf" else ()

    # Unknown non-documentation inputs are conservatively routed to the whole
    # framework instead of being treated as no-build changes.
    return inventory.idf_projects if framework == "esp-idf" else inventory.arduino_sketches


def _build_matrix(framework: str, selected: Iterable[str]) -> tuple[dict[str, str], ...]:
    ordered = tuple(sorted(set(selected), key=str.casefold))
    if framework == "esp-idf":
        return tuple(
            {
                "name": PurePosixPath(project).name,
                "project": project,
                "idf_version": version,
                "idf_image": IDF_IMAGES[version],
            }
            for project in ordered
            for version in IDF_VERSIONS
        )
    return tuple({"name": PurePosixPath(sketch).name, "sketch": sketch} for sketch in ordered)


def select_from_changes(
    framework: str,
    inventory: Inventory,
    changed_paths: Sequence[str],
) -> Selection:
    selected: set[str] = set()
    firmware_touched = False
    for path in changed_paths:
        normalized = normalize_path(path)
        firmware_touched = firmware_touched or normalized == "firmware" or normalized.startswith("firmware/")
        selected.update(_route_path(normalized, framework, inventory))

    ordered = tuple(sorted(selected, key=str.casefold))
    return Selection(
        builds=_build_matrix(framework, ordered),
        selected=ordered,
        firmware_touched=firmware_touched,
        changed_paths=tuple(changed_paths),
    )


def select_manual(framework: str, inventory: Inventory, requested: str) -> Selection:
    candidates = inventory.idf_projects if framework == "esp-idf" else inventory.arduino_sketches
    value = normalize_path(requested) if requested.strip() not in {"", "all"} else "all"
    if value == "all":
        selected = candidates
    else:
        selected = tuple(
            candidate
            for candidate in candidates
            if value in {candidate, PurePosixPath(candidate).name}
        )
        if not selected:
            raise RoutingError(f"Unknown {framework} target: {requested!r}")

    return Selection(
        builds=_build_matrix(framework, selected),
        selected=tuple(selected),
        firmware_touched=False,
        changed_paths=(),
    )


def selection_payload(selection: Selection) -> dict[str, object]:
    return {
        "builds": list(selection.builds),
        "has_builds": bool(selection.builds),
        "selected": list(selection.selected),
        "firmware_touched": selection.firmware_touched,
        "changed_paths": list(selection.changed_paths),
    }


def write_github_output(path: Path, selection: Selection) -> None:
    payload = selection_payload(selection)
    with path.open("a", encoding="utf-8", newline="\n") as stream:
        for key in ("builds", "selected"):
            stream.write(f"{key}={json.dumps(payload[key], separators=(',', ':'))}\n")
        stream.write(f"has_builds={'true' if payload['has_builds'] else 'false'}\n")
        stream.write(
            f"firmware_touched={'true' if payload['firmware_touched'] else 'false'}\n"
        )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--framework", choices=("esp-idf", "arduino"), required=True)
    scope = parser.add_mutually_exclusive_group(required=True)
    scope.add_argument("--all", action="store_true", help="select a manual target or all targets")
    scope.add_argument("--changed-files-from", type=Path, help="Git name-status file")
    parser.add_argument("--requested", default="all", help="manual target name or path")
    parser.add_argument(
        "--github-output",
        type=Path,
        default=Path(os.environ["GITHUB_OUTPUT"]) if os.environ.get("GITHUB_OUTPUT") else None,
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = args.root.resolve()
        inventory = discover_inventory(root)
        if args.all:
            selection = select_manual(args.framework, inventory, args.requested)
        else:
            changed_paths = parse_name_status(args.changed_files_from)
            selection = select_from_changes(args.framework, inventory, changed_paths)
        if args.github_output:
            write_github_output(args.github_output, selection)
        print(json.dumps(selection_payload(selection), indent=2, sort_keys=True))
    except (OSError, RoutingError) as exc:
        print(f"CI routing error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
