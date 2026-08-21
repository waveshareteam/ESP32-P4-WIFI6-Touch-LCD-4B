#!/usr/bin/env python3
"""Write a complete Git name-status scope for conditional CI routing."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Sequence


FULL_OBJECT_ID = re.compile(r"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}")


class DiffScopeError(RuntimeError):
    """Raised when CI cannot prove that its changed-file scope is complete."""


def _validate_object_id(value: str, label: str) -> str:
    candidate = value.strip()
    if not FULL_OBJECT_ID.fullmatch(candidate):
        raise DiffScopeError(f"{label} must be a full Git object ID, got {value!r}.")
    return candidate


def _commit_exists(root: Path, object_id: str) -> bool:
    completed = subprocess.run(
        ["git", "-C", str(root), "cat-file", "-e", f"{object_id}^{{commit}}"],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return completed.returncode == 0


def _is_root_commit(root: Path, object_id: str) -> bool:
    completed = subprocess.run(
        ["git", "-C", str(root), "rev-list", "--parents", "-n", "1", object_id],
        check=False,
        capture_output=True,
    )
    return completed.returncode == 0 and len(completed.stdout.split()) == 1


def collect_name_status(root: Path, base: str, head: str) -> bytes:
    """Return a NUL-delimited, rename-aware diff or fail closed."""

    base_id = _validate_object_id(base, "base")
    head_id = _validate_object_id(head, "head")
    if not _commit_exists(root, head_id):
        raise DiffScopeError(f"head commit is unavailable: {head_id}")

    if set(base_id) == {"0"}:
        if not _is_root_commit(root, head_id):
            raise DiffScopeError(
                "an all-zero base is valid only when head is the repository root commit"
            )
        command = [
            "git",
            "-C",
            str(root),
            "diff-tree",
            "--root",
            "--no-commit-id",
            "--name-status",
            "-z",
            "-r",
            head_id,
        ]
    else:
        if not _commit_exists(root, base_id):
            raise DiffScopeError(f"base commit is unavailable: {base_id}")
        command = [
            "git",
            "-C",
            str(root),
            "diff",
            "--name-status",
            "-z",
            "--find-renames",
            base_id,
            head_id,
        ]

    completed = subprocess.run(command, check=False, capture_output=True)
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", errors="replace").strip()
        raise DiffScopeError(detail or "Git could not compute the changed-file scope.")
    if not completed.stdout:
        raise DiffScopeError("Git returned an empty changed-file scope.")
    return completed.stdout


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        payload = collect_name_status(args.root.resolve(), args.base, args.head)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(payload)
    except (DiffScopeError, OSError) as exc:
        print(f"CI diff scope error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
