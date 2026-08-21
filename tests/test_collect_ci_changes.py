from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "collect_ci_changes.py"
SPEC = importlib.util.spec_from_file_location("collect_ci_changes", SCRIPT)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = collector
SPEC.loader.exec_module(collector)


class CollectCiChangesTests(unittest.TestCase):
    def git(self, root: Path, *arguments: str) -> str:
        completed = subprocess.run(
            ["git", "-C", str(root), *arguments],
            check=True,
            capture_output=True,
            text=True,
        )
        return completed.stdout.strip()

    def make_repository(self, root: Path) -> tuple[str, str]:
        self.git(root, "init", "--initial-branch=main")
        self.git(root, "config", "user.name", "CI Test")
        self.git(root, "config", "user.email", "ci@example.invalid")
        source = root / "examples/esp-idf/demo/main/main.c"
        source.parent.mkdir(parents=True)
        source.write_text("int first;\n", encoding="utf-8")
        self.git(root, "add", ".")
        self.git(root, "commit", "-m", "initial")
        initial = self.git(root, "rev-parse", "HEAD")
        source.write_text("int second;\n", encoding="utf-8")
        self.git(root, "commit", "-am", "change")
        return initial, self.git(root, "rev-parse", "HEAD")

    def test_initial_push_uses_root_diff(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            initial, _ = self.make_repository(root)
            payload = collector.collect_name_status(root, "0" * 40, initial)
        self.assertIn(b"examples/esp-idf/demo/main/main.c", payload)

    def test_zero_base_rejects_a_non_root_branch_head(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, head = self.make_repository(root)
            with self.assertRaises(collector.DiffScopeError):
                collector.collect_name_status(root, "0" * 40, head)

    def test_complete_range_is_nul_delimited(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            initial, head = self.make_repository(root)
            payload = collector.collect_name_status(root, initial, head)
        self.assertIn(b"M\0examples/esp-idf/demo/main/main.c\0", payload)

    def test_unavailable_or_invalid_scope_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            initial, head = self.make_repository(root)
            for base, candidate_head in (
                ("", head),
                ("1" * 40, head),
                (initial, "2" * 40),
            ):
                with self.subTest(base=base, head=candidate_head):
                    with self.assertRaises(collector.DiffScopeError):
                        collector.collect_name_status(root, base, candidate_head)

    def test_empty_git_range_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, head = self.make_repository(root)
            with self.assertRaises(collector.DiffScopeError):
                collector.collect_name_status(root, head, head)

    def test_exact_workflow_cli_contract(self) -> None:
        expected = (
            "python scripts/collect_ci_changes.py \\\n"
            "            --base \"$BASE_SHA\" \\\n"
            "            --head \"$HEAD_SHA\" \\\n"
            "            --output \"$changed_file\""
        )
        for name in ("esp-idf.yml", "arduino.yml"):
            text = (ROOT / ".github/workflows" / name).read_text(encoding="utf-8")
            self.assertIn(expected, text)
            self.assertNotIn("git diff-tree --root", text)


if __name__ == "__main__":
    unittest.main()
