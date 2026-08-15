#!/usr/bin/env python3
"""Run repository-local documentation, privacy, and CI policy checks."""

from __future__ import annotations

import argparse
import csv
import html
import json
import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT_DOCUMENTS = ("README.md", "SUPPORT.md", "CONTRIBUTING.md", "THIRD_PARTY_NOTICES.md")
README_QUICK_LINKS = (
    ("🌐", "Product", "https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm"),
    ("📚", "Documentation", "https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B"),
    ("📦", "Firmware", "docs/firmware.md"),
    ("🚀", "Quick start", "docs/getting-started.md"),
    ("🧩", "ESP-IDF", "examples/esp-idf/README.md"),
    ("🔧", "Arduino", "examples/arduino/README.md"),
)
README_ZH_QUICK_LINKS = (
    ("🌐", "产品页面", "https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm"),
    ("📚", "产品文档", "https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4B"),
    ("📦", "固件", "docs/firmware_ZH.md"),
    ("🚀", "快速开始", "docs/getting-started_ZH.md"),
    ("🧩", "ESP-IDF", "examples/esp-idf/README_ZH.md"),
    ("🔧", "Arduino", "examples/arduino/README_ZH.md"),
)
README_HERO_PATH = "docs/assets/esp32-p4-wifi6-touch-lcd-4b.jpg"
README_HERO_ALTS = {
    "README.md": "ESP32-P4-WIFI6-Touch-LCD-4B 4-inch touch display",
    "README_ZH.md": "ESP32-P4-WIFI6-Touch-LCD-4B 4 英寸触摸显示屏",
}
README_WORKFLOW_BADGES = (
    ("Repository Policy", "repository-policy.yml"),
    ("ESP-IDF Build", "esp-idf.yml"),
    ("Arduino Build", "arduino.yml"),
    ("Firmware Build", "firmware.yml"),
)
README_H2_ICONS = ("🖥️", "🗂️", "🧪", "🚀", "📦", "📄")
MANAGED_BSP_COMPONENT = "waveshare/esp32_p4_wifi6_touch_lcd_4b"
MANAGED_BSP_VERSION = "3.0.0"
FORBIDDEN_LOCAL_REUSABLE_COMPONENTS = (
    "firmware/brookesia/components/esp32_p4_wifi6_touch_lcd_4b",
    "firmware/brookesia/components/esp_lcd_st7703",
)
GITHUB_WORKFLOW_URL = "https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4B/actions/workflows/{}"
ICON_SYNCHRONIZED_PAIRS = (
    ("README.md", "README_ZH.md"),
    ("SUPPORT.md", "SUPPORT_ZH.md"),
    ("docs/ci.md", "docs/ci_ZH.md"),
)
MARKDOWN_LINK_RE = re.compile(r"!?\[([^\]]*)\]\(([^)]+)\)")
HTML_HREF_RE = re.compile(r"\bhref=[\"']([^\"']+)[\"']", re.IGNORECASE)
HTML_IMAGE_RE = re.compile(r"<img\b(?P<attributes>[^>]*)>", re.IGNORECASE)
HTML_ATTRIBUTE_RE = re.compile(r"\b(?P<name>[a-zA-Z0-9_-]+)=[\"'](?P<value>[^\"']*)[\"']")
HEADING_RE = re.compile(r"^ {0,3}#{1,6}\s+(.+?)\s*#*\s*$")
H2_RE = re.compile(r"^##\s+(\S+)", re.MULTILINE)
FENCE_RE = re.compile(r"^ {0,3}(```|~~~)")
PRIVATE_TEXT_PATTERNS = (
    (
        "Windows user/workspace path",
        re.compile(r"\b[A-Za-z]:[\\/](?:Users|SourceCode|Projects|workspaces)[\\/]", re.IGNORECASE),
    ),
    ("POSIX home path", re.compile(r"/(?:home|Users)/[A-Za-z0-9._-]+/")),
    ("UNC share path", re.compile(r"(?:^|\s)\\\\[^\\\s]+\\[^\\\s]+")),
    ("local agent provenance", re.compile(r"(?:\.codex|\.agents)[/\\]", re.IGNORECASE)),
)


def tracked_markdown(root: Path) -> tuple[Path, ...]:
    """Return checked-in and untracked Markdown without scanning ignored build trees."""
    try:
        completed = subprocess.run(
            ["git", "-C", str(root), "ls-files", "--cached", "--others", "--exclude-standard", "-z", "--", "*.md"],
            check=True,
            capture_output=True,
        )
    except FileNotFoundError:
        ignored_generated = {".git", "build", "managed_components", "release-artifacts"}
        return tuple(path for path in root.rglob("*.md") if not ignored_generated.intersection(path.parts))
    paths = [item for item in completed.stdout.split(b"\0") if item]
    return tuple(candidate for item in paths if (candidate := root / item.decode("utf-8", errors="surrogateescape")).is_file())


def _is_chinese(path: Path) -> bool:
    return path.stem.endswith(("_ZH", "_CN"))


def _english_counterpart(path: Path) -> Path:
    if path.stem.endswith("_ZH"):
        return path.with_name(f"{path.stem[:-3]}.md")
    if path.stem.endswith("_CN"):
        return path.with_name(f"{path.stem[:-3]}.md")
    return path


def _chinese_counterpart(path: Path) -> Path:
    relative = path.as_posix()
    suffix = "_CN" if relative.endswith("firmware/brookesia/README.md") else "_ZH"
    return path.with_name(f"{path.stem}{suffix}.md")


def first_party_markdown(root: Path) -> tuple[Path, ...]:
    """Inventory only the repository-maintained bilingual Markdown surface.

    This intentionally avoids bundled libraries, components, managed components,
    third-party trees, nested examples, and Brookesia's embedded upstream trees.
    """
    paths: set[Path] = set()

    def add(path: Path) -> None:
        if path.is_file():
            paths.add(path)

    for name in ROOT_DOCUMENTS:
        add(root / name)
        add((root / name).with_name(f"{Path(name).stem}_ZH.md"))
    for path in (root / ".github/ISSUE_TEMPLATE").glob("*.md"):
        add(path)
    for name in ("pull_request_template.md", "pull_request_template_ZH.md"):
        add(root / ".github" / name)
    for directory in ("config", "hardware", "releases", "schematic"):
        add(root / directory / "README.md")
        add(root / directory / "README_ZH.md")
    for path in (root / "docs").glob("*.md"):
        add(path)
    for framework in ("arduino", "esp-idf"):
        example_root = root / "examples" / framework
        for name in ("README.md", "README_ZH.md"):
            add(example_root / name)
        if example_root.is_dir():
            for child in example_root.iterdir():
                if child.is_dir() and child.name != "libraries":
                    add(child / "README.md")
                    add(child / "README_ZH.md")
    for name in ("README.md", "README_CN.md"):
        add(root / "firmware/brookesia" / name)
    for name in ("README.md", "README_ZH.md"):
        add(root / "firmware/brookesia/archive" / name)
    return tuple(sorted(paths, key=lambda path: path.as_posix()))


def maintained_pairs(root: Path) -> tuple[tuple[Path, Path], ...]:
    """Return expected English/Chinese pairs, including a one-sided orphan."""
    pairs: dict[Path, Path] = {}
    for path in first_party_markdown(root):
        english = _english_counterpart(path) if _is_chinese(path) else path
        pairs[english] = _chinese_counterpart(english)
    return tuple(sorted(pairs.items(), key=lambda pair: pair[0].as_posix()))


def _link_value(raw: str) -> str:
    value = html.unescape(raw).strip()
    if value.startswith("<") and ">" in value:
        return value[1 : value.index(">")]
    if " \"" in value or " '" in value:
        return value.split(maxsplit=1)[0]
    return value


def _parse_local_target(raw: str) -> tuple[str, str] | None:
    value = _link_value(raw)
    if not value or value.startswith("//"):
        return None
    parsed = urlsplit(value)
    if parsed.scheme or parsed.netloc:
        return None
    return unquote(parsed.path), unquote(parsed.fragment)


def _iter_links(text: str) -> tuple[tuple[str, str], ...]:
    links = [(label, raw) for label, raw in MARKDOWN_LINK_RE.findall(text)]
    links.extend(("", raw) for raw in HTML_HREF_RE.findall(text))
    return tuple(links)


def _html_images(text: str) -> tuple[dict[str, str], ...]:
    return tuple(
        {match.group("name").lower(): html.unescape(match.group("value")) for match in HTML_ATTRIBUTE_RE.finditer(image.group("attributes"))}
        for image in HTML_IMAGE_RE.finditer(text)
    )


def _case_exact(path: Path) -> bool:
    resolved = path.resolve()
    anchor = Path(resolved.anchor)
    current = anchor
    for part in resolved.parts[1:]:
        if not current.is_dir() or part not in {entry.name for entry in current.iterdir()}:
            return False
        current /= part
    return current.exists()


def _resolve_local_target(root: Path, markdown: Path, raw_target: str) -> tuple[Path, str] | None:
    parsed = _parse_local_target(raw_target)
    if parsed is None:
        return None
    target, fragment = parsed
    candidate = (markdown.parent / target).resolve() if target else markdown.resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError:
        return candidate, fragment
    return candidate, fragment


def github_heading_slugs(text: str) -> set[str]:
    """Produce stable GitHub-style slugs for this repository's Markdown headings."""
    slugs: set[str] = set()
    fenced = False
    for line in text.splitlines():
        if FENCE_RE.match(line):
            fenced = not fenced
            continue
        if fenced:
            continue
        match = HEADING_RE.match(line)
        if not match:
            continue
        title = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", match.group(1))
        title = re.sub(r"<[^>]+>", "", title)
        title = html.unescape(title).lower()
        title = re.sub(r"[`*_~]", "", title)
        title = re.sub(r"[^\w\s-]", "", title, flags=re.UNICODE)
        slug = re.sub(r"\s+", "-", title.strip())
        candidate = slug
        suffix = 1
        while candidate in slugs:
            candidate = f"{slug}-{suffix}" if slug else f"-{suffix}"
            suffix += 1
        slugs.add(candidate)
    return slugs


def check_links(root: Path, markdown_files: tuple[Path, ...]) -> list[str]:
    errors: list[str] = []
    slug_cache: dict[Path, set[str]] = {}
    for markdown in markdown_files:
        text = markdown.read_text(encoding="utf-8")
        for _, raw_target in _iter_links(text):
            resolved = _resolve_local_target(root, markdown, raw_target)
            if resolved is None or "{{" in raw_target or "${" in raw_target:
                continue
            candidate, fragment = resolved
            name = markdown.relative_to(root).as_posix()
            try:
                candidate.relative_to(root.resolve())
            except ValueError:
                errors.append(f"{name}: local link escapes repository: {raw_target}")
                continue
            if not _case_exact(candidate):
                errors.append(f"{name}: missing or case-mismatched local link: {raw_target}")
                continue
            if fragment:
                if candidate.suffix.lower() != ".md":
                    errors.append(f"{name}: fragment target is not Markdown: {raw_target}")
                    continue
                slugs = slug_cache.setdefault(candidate, github_heading_slugs(candidate.read_text(encoding="utf-8")))
                if fragment not in slugs:
                    errors.append(f"{name}: missing Markdown fragment: {raw_target}")
    return errors


def _has_link_to(root: Path, source: Path, target: Path) -> bool:
    for _, raw_target in _iter_links(source.read_text(encoding="utf-8")):
        resolved = _resolve_local_target(root, source, raw_target)
        if resolved is not None and resolved[0] == target.resolve():
            return True
    return False


def _is_language_switch(source: Path, target: Path, label: str) -> bool:
    if target == (_english_counterpart(source) if _is_chinese(source) else _chinese_counterpart(source)):
        return True
    return label.strip().lower() in {"english", "chinese", "中文", "简体中文"}


def check_bilingual_contract(root: Path) -> list[str]:
    errors: list[str] = []
    for english_path, chinese_path in maintained_pairs(root):
        english_name = english_path.relative_to(root).as_posix()
        chinese_name = chinese_path.relative_to(root).as_posix()
        if not english_path.is_file() or not chinese_path.is_file():
            errors.append(f"Missing maintained bilingual pair: {english_name} / {chinese_name}")
            continue
        if not _has_link_to(root, english_path, chinese_path):
            errors.append(f"{english_name}: missing language switch to {chinese_name}")
        if not _has_link_to(root, chinese_path, english_path):
            errors.append(f"{chinese_name}: missing language switch to {english_name}")

    for english_name, chinese_name in ICON_SYNCHRONIZED_PAIRS:
        english_path, chinese_path = root / english_name, root / chinese_name
        if english_path.is_file() and chinese_path.is_file():
            english_icons = H2_RE.findall(english_path.read_text(encoding="utf-8"))
            chinese_icons = H2_RE.findall(chinese_path.read_text(encoding="utf-8"))
            if english_icons != chinese_icons:
                errors.append(f"Heading-icon sequence differs: {english_name}={english_icons}, {chinese_name}={chinese_icons}")

    for source in first_party_markdown(root):
        source_is_chinese = _is_chinese(source)
        for label, raw_target in _iter_links(source.read_text(encoding="utf-8")):
            resolved = _resolve_local_target(root, source, raw_target)
            if resolved is None:
                continue
            target, _ = resolved
            if not target.is_file() or target.suffix.lower() != ".md":
                continue
            if _is_language_switch(source, target, label):
                continue
            expected = _chinese_counterpart(target) if source_is_chinese else _english_counterpart(target)
            if expected != target and expected.is_file():
                source_name = source.relative_to(root).as_posix()
                errors.append(
                    f"{source_name}: wrong-language local link {raw_target}; expected {expected.relative_to(root).as_posix()}"
                )

    for name, links in (("README.md", README_QUICK_LINKS), ("README_ZH.md", README_ZH_QUICK_LINKS)):
        path = root / name
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        positions: list[int] = []
        for icon, label, target in links:
            marker = f">{icon} {label}</a>"
            position = text.find(marker)
            if position < 0 or f'href="{target}"' not in text[max(0, position - 120) : position + len(marker)]:
                errors.append(f"{name}: missing semantic quick link {icon} {label} -> {target}")
            positions.append(position)
        if positions != sorted(positions) or any(position < 0 for position in positions):
            errors.append(f"{name}: semantic quick-link icon sequence differs from policy")
        if "<div align=\"center\">" not in text or "<h1>" not in text:
            errors.append(f"{name}: missing centered single-product header")
    return errors


def check_homepage_contract(root: Path) -> list[str]:
    errors: list[str] = []
    hero_path = root / README_HERO_PATH
    if not hero_path.is_file():
        errors.append(f"Homepage hero is missing: {README_HERO_PATH}")

    for name, expected_alt in README_HERO_ALTS.items():
        path = root / name
        if not path.is_file():
            errors.append(f"Homepage is missing: {name}")
            continue
        text = path.read_text(encoding="utf-8")
        heroes = [image for image in _html_images(text) if image.get("src") == README_HERO_PATH]
        if len(heroes) != 1:
            errors.append(f"{name}: must contain exactly one local homepage hero at {README_HERO_PATH}")
        elif heroes[0].get("alt") != expected_alt or not heroes[0].get("alt", "").strip():
            errors.append(f"{name}: homepage hero alt must be the localized product description")

        positions: list[int] = []
        for label, workflow in README_WORKFLOW_BADGES:
            workflow_url = GITHUB_WORKFLOW_URL.format(workflow)
            marker = f'<a href="{workflow_url}"><img src="{workflow_url}/badge.svg" alt="{label}"></a>'
            positions.append(text.find(marker))
        if any(position < 0 for position in positions) or positions != sorted(positions):
            errors.append(f"{name}: workflow badges must be present in Repository Policy, ESP-IDF, Arduino, Firmware order")

    try:
        config = json.loads((root / "config/markdown-audit.json").read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return errors + [f"config/markdown-audit.json: cannot read homepage audit contract: {exc}"]
    pairs = config.get("homepage_pairs")
    if not isinstance(pairs, list):
        return errors + ["config/markdown-audit.json: homepage_pairs must declare the homepage contract"]
    homepage = next(
        (
            pair
            for pair in pairs
            if isinstance(pair, dict) and pair.get("english") == "README.md" and pair.get("chinese") == "README_ZH.md"
        ),
        None,
    )
    if not isinstance(homepage, dict):
        return errors + ["config/markdown-audit.json: README homepage pair is missing"]
    if homepage.get("profile") != "single-product":
        errors.append("config/markdown-audit.json: README homepage profile must be single-product")
    if "hero_image" not in homepage.get("required_components", []):
        errors.append("config/markdown-audit.json: README homepage must require hero_image")
    if homepage.get("required_quick_links") != ["product", "documentation", "firmware", "quick_start", "esp_idf", "arduino"]:
        errors.append("config/markdown-audit.json: README quick-link contract differs from policy")
    if "build" not in homepage.get("required_badges", []):
        errors.append("config/markdown-audit.json: README homepage must require a build badge")
    if homepage.get("required_h2_icons") != list(README_H2_ICONS):
        errors.append("config/markdown-audit.json: README H2-icon contract differs from policy")
    return errors


def check_public_text(root: Path, markdown_files: tuple[Path, ...]) -> list[str]:
    errors: list[str] = []
    for markdown in markdown_files:
        text = markdown.read_text(encoding="utf-8")
        for label, pattern in PRIVATE_TEXT_PATTERNS:
            if pattern.search(text):
                errors.append(f"{markdown.relative_to(root).as_posix()}: contains {label}")
    return errors


def check_managed_component_contract(root: Path) -> list[str]:
    """Require Registry resolution for reusable board/display components."""
    errors: list[str] = []
    for relative in FORBIDDEN_LOCAL_REUSABLE_COMPONENTS:
        component_path = root / relative
        if component_path.is_file() or (
            component_path.is_dir()
            and any(candidate.is_file() for candidate in component_path.rglob("*"))
        ):
            errors.append(f"{relative}: local reusable component shadows ESP Component Registry")

    manifest_path = root / "firmware/brookesia/components/bsp_extra/idf_component.yml"
    if not manifest_path.is_file():
        return errors + [
            "firmware/brookesia/components/bsp_extra/idf_component.yml: managed BSP dependency manifest is missing"
        ]
    manifest = manifest_path.read_text(encoding="utf-8")
    dependency = re.compile(
        rf"(?m)^  {re.escape(MANAGED_BSP_COMPONENT)}:\s*\n"
        rf"    version: [\"']?{re.escape(MANAGED_BSP_VERSION)}[\"']?\s*$"
    )
    if not dependency.search(manifest):
        errors.append(
            "firmware/brookesia/components/bsp_extra/idf_component.yml: "
            f"must pin Registry {MANAGED_BSP_COMPONENT} {MANAGED_BSP_VERSION}"
        )
    return errors


def check_ci_contract(root: Path) -> list[str]:
    errors: list[str] = []
    idf = (root / ".github/workflows/esp-idf.yml").read_text(encoding="utf-8")
    arduino = (root / ".github/workflows/arduino.yml").read_text(encoding="utf-8")
    firmware = (root / ".github/workflows/firmware.yml").read_text(encoding="utf-8")
    policy = (root / ".github/workflows/repository-policy.yml").read_text(encoding="utf-8")
    for name, text in (("esp-idf.yml", idf), ("arduino.yml", arduino)):
        if "scripts/select_ci_targets.py" not in text:
            errors.append(f"{name}: changed-file selector is not invoked")
        if re.search(r"^\s+paths(?:-ignore)?:", text, re.MULTILINE):
            errors.append(f"{name}: path filters hide the always-visible routing job")
    if '. "$IDF_PATH/export.sh"' not in idf:
        errors.append("esp-idf.yml: IDF container environment is not activated")
    if "matrix.idf_image" not in idf or "@sha256:" not in firmware:
        errors.append("IDF workflows: container images are not pinned by digest")
    if "firmware/brookesia" in idf:
        errors.append("esp-idf.yml: maintained firmware leaked into default example CI")
    if "workflow_dispatch:" not in firmware or "pull_request:" not in firmware or "push:" in firmware or '!firmware/brookesia/**/*.md' not in firmware:
        errors.append("firmware.yml: maintained firmware must remain separate, PR source-impact gated, and manually dispatchable")
    if '. "$IDF_PATH/export.sh"' not in firmware:
        errors.append("firmware.yml: IDF container environment is not activated")
    package_script = root / "scripts/package_ci_firmware.py"
    flasher_script = root / "scripts/Flash-CI-Firmware.ps1"
    flasher_cmd = root / "Flash-CI-Firmware.cmd"
    flasher_core = root / "scripts/ci_firmware.py"
    flasher_sh = root / "Flash-CI-Firmware.sh"
    if not package_script.is_file() or not flasher_script.is_file() or not flasher_cmd.is_file() or not flasher_core.is_file() or not flasher_sh.is_file():
        errors.append("CI firmware packaging: required packager, shared core, or platform wrapper is missing")
    else:
        packager_text = package_script.read_text(encoding="utf-8")
        flasher_text = flasher_core.read_text(encoding="utf-8")
        if "schema_version\": 1" not in packager_text or "c6_firmware_included" not in packager_text or "metadata_path" not in packager_text or "ARDUINO_FQBN" not in packager_text:
            errors.append("package_ci_firmware.py: schema-1 P4/C6 manifest contract is missing")
        if "Hash of data verified" not in flasher_text or "write_flash" not in flasher_text or "erase_flash" in flasher_text or "safe_extract" not in flasher_text or "assert_repository_unchanged" not in flasher_text or "validate_idf_metadata" not in flasher_text:
            errors.append("ci_firmware.py: direct verified non-erasing flash contract is missing")
        if "-STA -File" not in flasher_cmd.read_text(encoding="utf-8"):
            errors.append("Flash-CI-Firmware.cmd: STA PowerShell forwarding contract is missing")
        if "ci_firmware.py" not in flasher_script.read_text(encoding="utf-8") or "ci_firmware.py" not in flasher_sh.read_text(encoding="utf-8"):
            errors.append("platform wrappers must forward to the shared CI firmware core")
        if "run-id" in flasher_text or "RunId" in flasher_script.read_text(encoding="utf-8"):
            errors.append("CI firmware flasher must not allow manual workflow-run selection")
    artifact_sha = "actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a"
    final_ref = "ref: ${{ github.event.pull_request.head.sha || github.sha }}"
    for workflow_name, text, artifact in (
        ("esp-idf.yml", idf, "firmware-esp-idf-${{ matrix.name }}-${{ matrix.idf_version }}-rev1_3"),
        ("arduino.yml", arduino, "firmware-arduino-${{ matrix.name }}-3.3.11-rev1_3"),
        ("firmware.yml", firmware, "firmware-brookesia-v5.5.5-${{ matrix.profile }}"),
    ):
        if final_ref not in text:
            errors.append(f"{workflow_name}: build checkout is not pinned to the event final SHA")
        if artifact_sha not in text or artifact not in text or "retention-days: 14" not in text or "if-no-files-found: error" not in text:
            errors.append(f"{workflow_name}: CI firmware artifact upload contract is incomplete")
        if "PACKAGE_GIT_SHA: ${{ github.event.pull_request.head.sha || github.sha }}" not in text:
            errors.append(f"{workflow_name}: package SHA is not bound to the final checkout SHA")
    if "--export-binaries" not in arduino:
        errors.append("arduino.yml: compile must export deterministic package binaries")
    for workflow_name, text in (("esp-idf.yml", idf), ("arduino.yml", arduino), ("firmware.yml", firmware), ("repository-policy.yml", policy)):
        for reference in re.findall(r"\buses:\s+[^@\s]+@([^\s#]+)", text):
            if not re.fullmatch(r"[0-9a-f]{40}", reference):
                errors.append(f"{workflow_name}: Action is not pinned to a full commit SHA")
    return errors


def check_revision_profile_contract(root: Path) -> list[str]:
    """Keep the revision split explicit without multiplying the example matrices."""
    errors: list[str] = []
    shared = (root / "config/sdkconfig.defaults").read_text(encoding="utf-8")
    if not re.search(r"^CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y$", shared, re.MULTILINE) or not re.search(r"^CONFIG_ESP32P4_REV_MIN_100=y$", shared, re.MULTILINE):
        errors.append("config/sdkconfig.defaults: shared ESP-IDF default must be pre-v3 with a 1.x minimum")
    usb = (root / "examples/esp-idf/usb-extended-screen/sdkconfig.defaults.esp32p4").read_text(encoding="utf-8")
    if "CONFIG_ESP32P4_REV_MIN_1=" in usb:
        errors.append("usb-extended-screen: obsolete local revision default conflicts with the shared contract")
    arduino = (root / ".github/workflows/arduino.yml").read_text(encoding="utf-8")
    if "ChipVariant=prev3" not in arduino:
        errors.append("arduino.yml: Arduino default must remain ChipVariant=prev3")
    firmware = (root / ".github/workflows/firmware.yml").read_text(encoding="utf-8")
    for required in ("profile: rev1_3", "profile: rev3_x", "$GITHUB_WORKSPACE/firmware/brookesia/build-${{ matrix.profile }}", "$RUNNER_TEMP/brookesia-${{ matrix.profile }}.sdkconfig", "$GITHUB_WORKSPACE/firmware/brookesia/sdkconfig.defaults;$GITHUB_WORKSPACE/firmware/brookesia/${{ matrix.defaults }}", "firmware-brookesia-v5.5.5-${{ matrix.profile }}"):
        if required not in firmware:
            errors.append(f"firmware.yml: missing isolated dual-profile contract {required}")
    base_defaults = (root / "firmware/brookesia/sdkconfig.defaults").read_text(encoding="utf-8")
    if any(symbol not in base_defaults for symbol in ("CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y", "CONFIG_ESP32P4_REV_MIN_100=y", "CONFIG_SPIRAM_SPEED_200M=y", "CONFIG_SPIRAM_SPEED=200")) or "CONFIG_SPIRAM_SPEED_250M=y" in base_defaults or "CONFIG_SPIRAM_SPEED=250" in base_defaults:
        errors.append("firmware/brookesia/sdkconfig.defaults: base default must be rev1.3 with only 200 MHz PSRAM")
    if not re.search(r"^CONFIG_PARTITION_TABLE_OFFSET=0x8000$", base_defaults, re.MULTILINE):
        errors.append("firmware/brookesia/sdkconfig.defaults: base default must keep the 0x8000 partition-table offset")
    for profile, symbols in {
        "rev1_3": ("CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y", "CONFIG_ESP32P4_REV_MIN_100=y", "CONFIG_SPIRAM_SPEED_200M=y"),
        "rev3_x": ("CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n", "CONFIG_ESP32P4_REV_MIN_300=y", "CONFIG_SPIRAM_SPEED_250M=y", "CONFIG_BOOTLOADER_LOG_LEVEL_ERROR=y", "CONFIG_BOOTLOADER_LOG_LEVEL=1"),
    }.items():
        defaults = (root / f"firmware/brookesia/sdkconfig.defaults.{profile}").read_text(encoding="utf-8")
        if any(symbol not in defaults for symbol in symbols):
            errors.append(f"firmware/brookesia/sdkconfig.defaults.{profile}: profile symbols are incomplete")
    partitions = root / "firmware/brookesia/partitions.csv"
    expected_layout = {
        "nvsfactory": ("data", "nvs", "", "200K"),
        "nvs": ("data", "nvs", "", "840K"),
        "otadata": ("data", "ota", "", "0x2000"),
        "phy_init": ("data", "phy", "", "0x1000"),
        "model": ("data", "spiffs", "", "0xF0000"),
        "factory": ("app", "factory", "0x00200000", "8M"),
        "storage": ("data", "spiffs", "", "6M"),
    }
    actual_layout: dict[str, tuple[str, str, str, str]] = {}
    for row in csv.reader(partitions.read_text(encoding="utf-8").splitlines(), skipinitialspace=True):
        if not row or row[0].strip().startswith("#"):
            continue
        if len(row) >= 5:
            actual_layout[row[0].strip()] = tuple(field.strip() for field in row[1:5])
    if actual_layout != expected_layout:
        errors.append("firmware/brookesia/partitions.csv: factory offset and implicit-layout sizes must remain unchanged")
    packager = (root / "scripts/package_ci_firmware.py").read_text(encoding="utf-8")
    flasher = (root / "scripts/ci_firmware.py").read_text(encoding="utf-8")
    for required in ("BOARD_PROFILES", '"rev1_3"', '"rev3_x"', "validate_idf_profile", "--board-profile"):
        if required not in packager:
            errors.append(f"package_ci_firmware.py: missing profile contract {required}")
    for required in ("expected_items", "profile_for_major", "validate_manifest", "safe_extract", "Hash of data verified", "PCB/electrical revision", "no-auto-next=ok"):
        if required not in flasher:
            errors.append(f"ci_firmware.py: missing profile safety contract {required}")
    return errors


def check_idf_partition_contract(root: Path) -> list[str]:
    errors: list[str] = []
    shared_defaults = (root / "config/sdkconfig.defaults").read_text(encoding="utf-8")
    if not re.search(r"^CONFIG_PARTITION_TABLE_OFFSET=0x10000$", shared_defaults, re.MULTILINE):
        errors.append("config/sdkconfig.defaults: first-party examples must reserve a 0x10000 partition-table offset")
    example_root = root / "examples/esp-idf"
    for csv_path in sorted(example_root.glob("**/*partition*.csv")):
        for line_number, raw_line in enumerate(csv_path.read_text(encoding="utf-8").splitlines(), start=1):
            if not raw_line.strip() or raw_line.lstrip().startswith("#"):
                continue
            fields = next(csv.reader([raw_line], skipinitialspace=True))
            if len(fields) >= 4 and fields[3].strip():
                errors.append(f"{csv_path.relative_to(root).as_posix()}:{line_number}: partition offset must be blank so it follows CONFIG_PARTITION_TABLE_OFFSET")
    return errors


def run_checks(root: Path) -> list[str]:
    markdown_files = tracked_markdown(root)
    errors: list[str] = []
    errors.extend(check_links(root, markdown_files))
    errors.extend(check_bilingual_contract(root))
    errors.extend(check_homepage_contract(root))
    errors.extend(check_public_text(root, markdown_files))
    errors.extend(check_managed_component_contract(root))
    errors.extend(check_ci_contract(root))
    errors.extend(check_revision_profile_contract(root))
    errors.extend(check_idf_partition_contract(root))
    if (root / "SECURITY.md").exists():
        errors.append("SECURITY.md exists although no verified private vulnerability-reporting endpoint is configured")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        errors = run_checks(root)
    except (OSError, subprocess.CalledProcessError, UnicodeError) as exc:
        print(f"Repository policy check failed to run: {exc}", file=sys.stderr)
        return 2
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        print(f"Repository policy: {len(errors)} error(s)", file=sys.stderr)
        return 1
    print("Repository policy: links, fragments, maintained bilingual pages, public text, and CI boundaries passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
