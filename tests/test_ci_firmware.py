from __future__ import annotations

import importlib.util
import json
import os
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "package_ci_firmware.py"
SPEC = importlib.util.spec_from_file_location("package_ci_firmware", SCRIPT)
assert SPEC and SPEC.loader
packager = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = packager
SPEC.loader.exec_module(packager)


class CiFirmwarePackageTests(unittest.TestCase):
    def fixture(self, root: Path) -> tuple[Path, Path]:
        project = root / "examples" / "esp-idf" / "hello_world"
        build = project / "build"
        build.mkdir(parents=True)
        (build / "bootloader.bin").write_bytes(b"boot")
        (build / "app.bin").write_bytes(b"application")
        (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "bootloader.bin", "0x10000": "app.bin"}}), encoding="utf-8")
        return project, build

    def test_idf_bundle_has_schema_one_safe_relative_manifest_and_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.fixture(root); output = root / "artifact.zip"
            old = Path.cwd(); os.chdir(root)
            try:
                packager.package_idf(project, build, "v5.5.5", output)
            finally:
                os.chdir(old)
            with zipfile.ZipFile(output) as archive:
                document = json.loads(archive.read("manifest.json"))
                self.assertIn("metadata/flasher_args.json", archive.namelist())
            self.assertEqual(1, document["schema_version"])
            self.assertEqual("examples/esp-idf/hello_world", document["source_project"])
            self.assertEqual("esp32p4", document["chip"])
            self.assertEqual(32 * 1024 * 1024, document["flash"]["size_bytes"])
            self.assertFalse(document["c6_firmware_included"])
            self.assertNotIn("erase_flash", document["flash"]["command"])
            self.assertEqual([], document["idf_write_flash_args"])

    def test_rejects_unsafe_idf_path_and_overlapping_ranges(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.fixture(root); old = Path.cwd(); os.chdir(root)
            try:
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "../escape.bin"}}), encoding="utf-8")
                with self.assertRaises(ValueError): packager.package_idf(project, build, "v5.5.5", root / "bad.zip")
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "bootloader.bin"}, "write_flash_args": ["--erase-all"]}), encoding="utf-8")
                with self.assertRaises(ValueError): packager.package_idf(project, build, "v5.5.5", root / "dangerous.zip")
                with self.assertRaises(ValueError): packager.validate_plan([{"offset": "0x0", "size": 8}, {"offset": "0x4", "size": 8}])
                with self.assertRaises(ValueError): packager.validate_plan([{"offset": "0x1fffff0", "size": 32}])
            finally:
                os.chdir(old)

    def test_arduino_requires_one_unambiguous_flash_layout_and_safe_fqbn(self) -> None:
        fqbn = "esp32:esp32:esp32p4:FlashSize=32M,ChipVariant=prev3,EraseFlash=none"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project = root / "examples" / "arduino" / "HelloWorld"; build = root / "build"; project.mkdir(parents=True); build.mkdir()
            for name in ("HelloWorld.ino.merged.bin", "another-merged.bin"): (build / name).write_bytes(b"x")
            old = Path.cwd(); os.chdir(root)
            try:
                with self.assertRaises(ValueError): packager.package_arduino(project, build, "3.3.11", root / "bad.zip", fqbn)
                with self.assertRaises(ValueError): packager.package_arduino(project, build, "3.3.11", root / "bad2.zip", "FlashSize=16M")
                with self.assertRaises(ValueError): packager.parse_arduino_fqbn("esp32:esp32:esp32p4:FlashSize=32M,FlashSize=32M,ChipVariant=prev3,EraseFlash=none")
            finally:
                os.chdir(old)

    def test_arduino_merged_bundle_has_exact_fqbn_and_offset_zero(self) -> None:
        fqbn = "esp32:esp32:esp32p4:FlashSize=32M,ChipVariant=prev3,EraseFlash=none"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project = root / "examples" / "arduino" / "HelloWorld"; build = root / "build"; project.mkdir(parents=True); build.mkdir()
            (build / "HelloWorld.ino.merged.bin").write_bytes(b"merged")
            old = Path.cwd(); os.chdir(root)
            try:
                output = packager.package_arduino(project, build, "3.3.11", root / "arduino.zip", fqbn)
            finally:
                os.chdir(old)
            with zipfile.ZipFile(output) as archive:
                document = json.loads(archive.read("manifest.json"))
            self.assertEqual(fqbn, document["fqbn"])
            self.assertEqual(["0x0"], [entry["offset"] for entry in document["files"]])
            self.assertEqual(460800, document["flash"]["baud"])
            self.assertIn("write_flash", document["flash"]["command"])
            self.assertNotIn("erase", document["flash"]["command"].lower())

    def test_ci_requires_full_package_sha(self) -> None:
        previous_ci, previous_sha = os.environ.get("CI"), os.environ.get("PACKAGE_GIT_SHA")
        os.environ["CI"] = "true"; os.environ["PACKAGE_GIT_SHA"] = "short"
        try:
            with self.assertRaises(ValueError): packager.git_sha()
        finally:
            if previous_ci is None: os.environ.pop("CI", None)
            else: os.environ["CI"] = previous_ci
            if previous_sha is None: os.environ.pop("PACKAGE_GIT_SHA", None)
            else: os.environ["PACKAGE_GIT_SHA"] = previous_sha

    def test_workflow_and_windows_safety_contract(self) -> None:
        for relative in (".github/workflows/esp-idf.yml", ".github/workflows/arduino.yml", ".github/workflows/firmware.yml"):
            text = (ROOT / relative).read_text(encoding="utf-8")
            self.assertIn("PACKAGE_GIT_SHA: ${{ github.event.pull_request.head.sha || github.sha }}", text)
            self.assertIn("actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a", text)
        arduino_workflow = (ROOT / ".github/workflows/arduino.yml").read_text(encoding="utf-8")
        self.assertIn("--export-binaries", arduino_workflow)
        flasher = (ROOT / "scripts" / "Flash-CI-Firmware.ps1").read_text(encoding="utf-8")
        self.assertIn("$Items.Count -ne 32", flasher)
        self.assertIn("Hash of data verified", flasher)
        self.assertIn("c6_firmware_included", flasher)
        self.assertIn("chip_id", flasher)
        self.assertIn("Test-ArduinoFqbn", flasher)
        self.assertIn("$manifest.flash.baud -ne 460800", flasher)
        self.assertNotIn("erase_flash", flasher)

    def test_list_only_matches_selector_inventory_and_artifact_contract(self) -> None:
        import subprocess

        completed = subprocess.run(
            ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "scripts/Flash-CI-Firmware.ps1"), "-ListOnly"],
            check=True, capture_output=True, text=True, encoding="utf-8",
        )
        lines = [line for line in completed.stdout.splitlines() if line[:1].isdigit()]
        self.assertEqual(32, len(lines))
        idf = [path.name for path in sorted((ROOT / "examples/esp-idf").iterdir()) if (path / "CMakeLists.txt").is_file()]
        arduino = [path.name for path in sorted((ROOT / "examples/arduino").iterdir()) if (path / f"{path.name}.ino").is_file()]
        expected = [f"firmware-esp-idf-{name}-{version}" for name in idf for version in ("v5.5.5", "v6.0.2")]
        expected += [f"firmware-arduino-{name}-3.3.11" for name in arduino]
        expected += ["firmware-brookesia-v5.5.5"]
        actual = [line.split(" artifact=", 1)[1].split(" source=", 1)[0] for line in lines]
        self.assertEqual(expected, actual)


if __name__ == "__main__":
    unittest.main()
