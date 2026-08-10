#!/usr/bin/env python3
"""Run repository-local documentation, privacy, and CI policy checks."""

from __future__ import annotations

import argparse
import csv
import html
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
ICON_SYNCHRONIZED_PAIRS = (
    ("README.md", "README_ZH.md"),
    ("SUPPORT.md", "SUPPORT_ZH.md"),
    ("docs/ci.md", "docs/ci_ZH.md"),
)
MARKDOWN_LINK_RE = re.compile(r"!?\[([^\]]*)\]\(([^)]+)\)")
HTML_HREF_RE = re.compile(r"\bhref=[\"']([^\"']+)[\"']", re.IGNORECASE)
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
    completed = subprocess.run(
        ["git", "-C", str(root), "ls-files", "--cached", "--others", "--exclude-standard", "-z", "--", "*.md"],
        check=True,
        capture_output=True,
    )
    paths = [item for item in completed.stdout.split(b"\0") if item]
    return tuple(
        candidate
        for item in paths
        if (candidate := root / item.decode("utf-8", errors="surrogateescape")).is_file()
    )


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


def check_public_text(root: Path, markdown_files: tuple[Path, ...]) -> list[str]:
    errors: list[str] = []
    for markdown in markdown_files:
        text = markdown.read_text(encoding="utf-8")
        for label, pattern in PRIVATE_TEXT_PATTERNS:
            if pattern.search(text):
                errors.append(f"{markdown.relative_to(root).as_posix()}: contains {label}")
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
    for workflow_name, text in (("esp-idf.yml", idf), ("arduino.yml", arduino), ("firmware.yml", firmware), ("repository-policy.yml", policy)):
        for reference in re.findall(r"\buses:\s+[^@\s]+@([^\s#]+)", text):
            if not re.fullmatch(r"[0-9a-f]{40}", reference):
                errors.append(f"{workflow_name}: Action is not pinned to a full commit SHA")
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
    errors.extend(check_public_text(root, markdown_files))
    errors.extend(check_ci_contract(root))
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
