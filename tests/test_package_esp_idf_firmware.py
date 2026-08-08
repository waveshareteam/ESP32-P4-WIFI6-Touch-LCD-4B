from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "package_esp_idf_firmware.py"
GIT_SHA = "0123456789abcdef0123456789abcdef01234567"


class FirmwarePackageTests(unittest.TestCase):
    def make_build_tree(self, root: Path, unsafe: bool = False) -> Path:
        build = root / "build"
        (build / "bootloader").mkdir(parents=True)
        (build / "partition_table").mkdir()
        (build / "bootloader" / "bootloader.bin").write_bytes(b"bootloader")
        (build / "partition_table" / "partition-table.bin").write_bytes(b"partition")
        (build / "application.bin").write_bytes(b"application")
        flash_files = {
            "0x0": "bootloader/bootloader.bin",
            "0x8000": "partition_table/partition-table.bin",
            "0x10000": "../outside.bin" if unsafe else "application.bin",
        }
        (build / "flasher_args.json").write_text(
            json.dumps(
                {
                    "extra_esptool_args": {
                        "chip": "esp32p4",
                        "before": "default_reset",
                        "after": "hard_reset",
                        "stub": True,
                    },
                    "write_flash_args": ["--flash_mode", "dio", "--flash_size", "32MB"],
                    "flash_files": flash_files,
                    "flash_settings": {"flash_mode": "dio", "flash_size": "32MB"},
                }
            ),
            encoding="utf-8",
        )
        (build / "project_description.json").write_text(
            json.dumps(
                {
                    "project_name": "package-test",
                    "project_version": "1.0.0",
                    "target": "esp32p4",
                    "git_revision": "v5.5.5",
                    "project_path": str(root / "examples" / "package-test"),
                }
            ),
            encoding="utf-8",
        )
        return build

    def run_package(self, build: Path, output: Path, *extra: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                str(build),
                "--output-dir",
                str(output),
                "--project",
                "package-test",
                "--idf-version",
                "v5.5.5",
                "--git-sha",
                GIT_SHA,
                *extra,
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_package_contains_complete_sorted_flash_set_and_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            build = self.make_build_tree(temp)
            output = temp / "artifacts"
            completed = self.run_package(build, output)
            self.assertEqual(0, completed.returncode, completed.stderr)
            archive = Path(completed.stdout.strip())
            self.assertTrue(archive.is_file())

            with zipfile.ZipFile(archive) as package:
                names = package.namelist()
                prefix = archive.stem
                expected_bins = (
                    f"{prefix}/bin/bootloader/bootloader.bin",
                    f"{prefix}/bin/partition_table/partition-table.bin",
                    f"{prefix}/bin/application.bin",
                )
                for name in (*expected_bins, f"{prefix}/manifest.json", f"{prefix}/flash.py"):
                    self.assertIn(name, names)
                manifest = json.loads(package.read(f"{prefix}/manifest.json"))
                self.assertEqual(2, manifest["package_format"])
                self.assertEqual("esp32p4", manifest["target"])
                self.assertEqual(GIT_SHA, manifest["git_sha"])
                self.assertEqual(["0x0", "0x8000", "0x10000"], [item["offset"] for item in manifest["files"]])
                for entry in manifest["files"]:
                    data = package.read(f"{prefix}/{entry['path']}")
                    self.assertEqual(len(data), entry["size"])
                    self.assertEqual(hashlib.sha256(data).hexdigest(), entry["sha256"])
                expected_pairs = [
                    value
                    for entry in manifest["files"]
                    for value in (entry["offset"], entry["path"])
                ]
                self.assertEqual(expected_pairs, manifest["flash_command"][-len(expected_pairs):])

    def test_unsafe_flash_path_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            build = self.make_build_tree(temp, unsafe=True)
            completed = self.run_package(build, temp / "artifacts")
            self.assertNotEqual(0, completed.returncode)
            self.assertIn("Unsafe flash file path", completed.stderr)

    def test_existing_archive_requires_explicit_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            build = self.make_build_tree(temp)
            output = temp / "artifacts"
            first = self.run_package(build, output)
            second = self.run_package(build, output)
            overwritten = self.run_package(build, output, "--overwrite")
            self.assertEqual(0, first.returncode, first.stderr)
            self.assertNotEqual(0, second.returncode)
            self.assertIn("Archive already exists", second.stderr)
            self.assertEqual(0, overwritten.returncode, overwritten.stderr)


if __name__ == "__main__":
    unittest.main()
