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


MAINTAINED_PAIRS = (
    ("README.md", "README_ZH.md"),
    ("SUPPORT.md", "SUPPORT_ZH.md"),
    ("docs/ci.md", "docs/ci_ZH.md"),
    ("firmware/brookesia/README.md", "firmware/brookesia/README_CN.md"),
)
README_QUICK_LINK_ICONS = ("🌐", "📚", "🚀", "🧩", "🔧")
MARKDOWN_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
HTML_HREF_RE = re.compile(r"\bhref=[\"']([^\"']+)[\"']", re.IGNORECASE)
H2_RE = re.compile(r"^##\s+(\S+)", re.MULTILINE)
PRIVATE_TEXT_PATTERNS = (
    (
        "Windows user/workspace path",
        re.compile(
            r"\b[A-Za-z]:[\\/](?:Users|SourceCode|Projects|workspaces)[\\/]",
            re.IGNORECASE,
        ),
    ),
    ("POSIX home path", re.compile(r"/(?:home|Users)/[A-Za-z0-9._-]+/")),
    ("UNC share path", re.compile(r"(?:^|\s)\\\\[^\\\s]+\\[^\\\s]+")),
    ("local agent provenance", re.compile(r"(?:\.codex|\.agents)[/\\]", re.IGNORECASE)),
)


def tracked_markdown(root: Path) -> tuple[Path, ...]:
    completed = subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "ls-files",
            "--cached",
            "--others",
            "--exclude-standard",
            "-z",
            "--",
            "*.md",
        ],
        check=True,
        capture_output=True,
    )
    paths = [item for item in completed.stdout.split(b"\0") if item]
    return tuple(
        candidate
        for item in paths
        if (candidate := root / item.decode("utf-8", errors="surrogateescape")).is_file()
    )


def _extract_local_target(raw: str) -> str | None:
    value = html.unescape(raw).strip()
    if value.startswith("<") and ">" in value:
        value = value[1 : value.index(">")]
    elif " \"" in value or " '" in value:
        value = value.split(maxsplit=1)[0]

    if not value or value.startswith(('#', '//')):
        return None
    parsed = urlsplit(value)
    if parsed.scheme or parsed.netloc:
        return None
    target = unquote(parsed.path)
    return target or None


def _case_exact(path: Path) -> bool:
    resolved = path.resolve()
    anchor = Path(resolved.anchor)
    current = anchor
    for part in resolved.parts[1:]:
        if not current.is_dir():
            return False
        names = {entry.name for entry in current.iterdir()}
        if part not in names:
            return False
        current /= part
    return current.exists()


def check_links(root: Path, markdown_files: tuple[Path, ...]) -> list[str]:
    errors: list[str] = []
    for markdown in markdown_files:
        text = markdown.read_text(encoding="utf-8")
        raw_targets = MARKDOWN_LINK_RE.findall(text) + HTML_HREF_RE.findall(text)
        for raw_target in raw_targets:
            target = _extract_local_target(raw_target)
            if target is None or "{{" in target or "${" in target:
                continue
            candidate = (markdown.parent / target).resolve()
            try:
                candidate.relative_to(root.resolve())
            except ValueError:
                errors.append(
                    f"{markdown.relative_to(root).as_posix()}: local link escapes repository: {raw_target}"
                )
                continue
            if not _case_exact(candidate):
                errors.append(
                    f"{markdown.relative_to(root).as_posix()}: missing or case-mismatched local link: {raw_target}"
                )
    return errors


def check_bilingual_contract(root: Path) -> list[str]:
    errors: list[str] = []
    for english_name, chinese_name in MAINTAINED_PAIRS:
        english_path = root / english_name
        chinese_path = root / chinese_name
        if not english_path.is_file() or not chinese_path.is_file():
            errors.append(f"Missing maintained bilingual pair: {english_name} / {chinese_name}")
            continue
        english = english_path.read_text(encoding="utf-8")
        chinese = chinese_path.read_text(encoding="utf-8")
        if Path(chinese_name).name not in english:
            errors.append(f"{english_name}: missing language switch to {chinese_name}")
        if Path(english_name).name not in chinese:
            errors.append(f"{chinese_name}: missing language switch to {english_name}")

        # The imported Brookesia pair predates the repository's _ZH convention
        # and is intentionally preserved while its much larger content sets are
        # reconciled. New maintained pairs must keep mirrored heading icons.
        if not chinese_name.endswith("_CN.md"):
            english_icons = H2_RE.findall(english)
            chinese_icons = H2_RE.findall(chinese)
            if english_icons != chinese_icons:
                errors.append(
                    f"Heading-icon sequence differs: {english_name}={english_icons}, "
                    f"{chinese_name}={chinese_icons}"
                )

    for name in ("README.md", "README_ZH.md"):
        text = (root / name).read_text(encoding="utf-8")
        if "<div align=\"center\">" not in text or "<h1>" not in text:
            errors.append(f"{name}: missing centered single-product header")
        for icon in README_QUICK_LINK_ICONS:
            if icon not in text:
                errors.append(f"{name}: missing semantic quick-link icon {icon}")
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
    if (
        "workflow_dispatch:" not in firmware
        or "pull_request:" not in firmware
        or "push:" in firmware
        or '!firmware/brookesia/**/*.md' not in firmware
    ):
        errors.append(
            "firmware.yml: maintained firmware must remain separate, PR source-impact gated, and manually dispatchable"
        )
    if '. "$IDF_PATH/export.sh"' not in firmware:
        errors.append("firmware.yml: IDF container environment is not activated")
    for workflow_name, text in (
        ("esp-idf.yml", idf),
        ("arduino.yml", arduino),
        ("firmware.yml", firmware),
        ("repository-policy.yml", policy),
    ):
        for reference in re.findall(r"\buses:\s+[^@\s]+@([^\s#]+)", text):
            if not re.fullmatch(r"[0-9a-f]{40}", reference):
                errors.append(f"{workflow_name}: Action is not pinned to a full commit SHA")
    return errors


def check_idf_partition_contract(root: Path) -> list[str]:
    errors: list[str] = []
    shared_defaults = (root / "config/sdkconfig.defaults").read_text(encoding="utf-8")
    if not re.search(
        r"^CONFIG_PARTITION_TABLE_OFFSET=0x10000$", shared_defaults, re.MULTILINE
    ):
        errors.append(
            "config/sdkconfig.defaults: first-party examples must reserve a 0x10000 partition-table offset"
        )

    example_root = root / "examples/esp-idf"
    for csv_path in sorted(example_root.glob("**/*partition*.csv")):
        for line_number, raw_line in enumerate(
            csv_path.read_text(encoding="utf-8").splitlines(), start=1
        ):
            if not raw_line.strip() or raw_line.lstrip().startswith("#"):
                continue
            fields = next(csv.reader([raw_line], skipinitialspace=True))
            if len(fields) >= 4 and fields[3].strip():
                errors.append(
                    f"{csv_path.relative_to(root).as_posix()}:{line_number}: "
                    "partition offset must be blank so it follows CONFIG_PARTITION_TABLE_OFFSET"
                )
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
        errors.append(
            "SECURITY.md exists although no verified private vulnerability-reporting endpoint is configured"
        )
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
    print("Repository policy: links, maintained bilingual pages, public text, and CI boundaries passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
