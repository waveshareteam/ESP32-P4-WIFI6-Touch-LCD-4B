from __future__ import annotations

import importlib.util
import json
import os
import shutil
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "package_ci_firmware.py"
CORE_SCRIPT = ROOT / "scripts" / "ci_firmware.py"
SELECTOR_SCRIPT = ROOT / "scripts" / "select_ci_targets.py"
SPEC = importlib.util.spec_from_file_location("package_ci_firmware", SCRIPT)
assert SPEC and SPEC.loader
packager = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = packager
SPEC.loader.exec_module(packager)
SELECTOR_SPEC = importlib.util.spec_from_file_location("select_ci_targets_for_firmware_test", SELECTOR_SCRIPT)
assert SELECTOR_SPEC and SELECTOR_SPEC.loader
selector = importlib.util.module_from_spec(SELECTOR_SPEC)
sys.modules[SELECTOR_SPEC.name] = selector
SELECTOR_SPEC.loader.exec_module(selector)
CORE_SPEC = importlib.util.spec_from_file_location("ci_firmware", CORE_SCRIPT)
assert CORE_SPEC and CORE_SPEC.loader
core = importlib.util.module_from_spec(CORE_SPEC)
sys.modules[CORE_SPEC.name] = core
CORE_SPEC.loader.exec_module(core)


class CiFirmwarePackageTests(unittest.TestCase):
    @staticmethod
    def expected_list_only_items() -> list[tuple[str, str]]:
        inventory = selector.discover_inventory(ROOT)
        expected = [
            (f"firmware-esp-idf-{Path(project).name}-{version}-rev1_3", project)
            for project in inventory.idf_projects
            for version in ("v5.5.5", "v6.0.2")
        ]
        expected += [
            (f"firmware-arduino-{Path(sketch).name}-3.3.11-rev1_3", sketch)
            for sketch in inventory.arduino_sketches
        ]
        return expected + [
            ("firmware-brookesia-v5.5.5-rev1_3", "firmware/brookesia"),
            ("firmware-brookesia-v5.5.5-rev3_x", "firmware/brookesia"),
        ]

    @staticmethod
    def list_only_powershell() -> str | None:
        return next((name for name in ("powershell.exe", "pwsh", "powershell") if shutil.which(name)), None)

    def assert_static_list_only_contract(self, expected: list[tuple[str, str]]) -> None:
        flasher = (ROOT / "scripts" / "Flash-CI-Firmware.ps1").read_text(encoding="utf-8")
        self.assertIn("ci_firmware.py", flasher)
        self.assertIn("$List -or $ListOnly", flasher)
        self.assertNotIn("Windows.Forms", flasher)
        self.assertEqual(expected, [(item.artifact, item.source_project) for item in core.expected_items(ROOT)])

    def fixture(self, root: Path) -> tuple[Path, Path]:
        project = root / "examples" / "esp-idf" / "hello_world"
        build = project / "build"
        build.mkdir(parents=True)
        (build / "bootloader.bin").write_bytes(b"boot")
        (build / "app.bin").write_bytes(b"application")
        (build / "config").mkdir()
        (build / "config" / "sdkconfig.json").write_text(json.dumps({"ESP32P4_SELECTS_REV_LESS_V3": True, "ESP32P4_REV_MIN_100": True}), encoding="utf-8")
        (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "bootloader.bin", "0x10000": "app.bin"}}), encoding="utf-8")
        return project, build

    def test_idf_bundle_has_schema_one_safe_relative_manifest_and_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.fixture(root); output = root / "artifact.zip"
            old = Path.cwd(); os.chdir(root)
            try:
                packager.package_idf(project, build, "v5.5.5", output, "rev1_3")
            finally:
                os.chdir(old)
            with zipfile.ZipFile(output) as archive:
                document = json.loads(archive.read("manifest.json"))
                self.assertIn("metadata/flasher_args.json", archive.namelist())
            self.assertEqual(1, document["schema_version"])
            self.assertEqual("examples/esp-idf/hello_world", document["source_project"])
            self.assertEqual("esp32p4", document["chip"])
            self.assertEqual("rev1_3", document["board_profile"])
            self.assertEqual("1.0", document["chip_revision"]["minimum"])
            self.assertEqual("3.0", document["chip_revision"]["maximum_exclusive"])
            self.assertEqual(32 * 1024 * 1024, document["flash"]["size_bytes"])
            self.assertFalse(document["c6_firmware_included"])
            self.assertNotIn("erase_flash", document["flash"]["command"])
            self.assertEqual([], document["idf_write_flash_args"])
            self.assertEqual(["bootloader.bin", "app.bin"], [entry["metadata_path"] for entry in document["files"]])

    def test_rejects_unsafe_idf_path_and_overlapping_ranges(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.fixture(root); old = Path.cwd(); os.chdir(root)
            try:
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "../escape.bin"}}), encoding="utf-8")
                with self.assertRaises(ValueError): packager.package_idf(project, build, "v5.5.5", root / "bad.zip", "rev1_3")
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "bootloader.bin"}, "write_flash_args": ["--erase-all"]}), encoding="utf-8")
                with self.assertRaises(ValueError): packager.package_idf(project, build, "v5.5.5", root / "dangerous.zip", "rev1_3")
                with self.assertRaises(ValueError): packager.validate_plan([{"offset": "0x0", "size": 8}, {"offset": "0x4", "size": 8}])
                with self.assertRaises(ValueError): packager.validate_plan([{"offset": "0x1fffff0", "size": 32}])
            finally:
                os.chdir(old)

    def test_idf_profile_accepts_prefixed_json_keys_and_rejects_the_wrong_profile(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.fixture(root)
            config = build / "config" / "sdkconfig.json"
            config.write_text(json.dumps({"CONFIG_ESP32P4_SELECTS_REV_LESS_V3": True, "CONFIG_ESP32P4_REV_MIN_100": True}), encoding="utf-8")
            self.assertEqual("y", packager.sdkconfig_values(build)["CONFIG_ESP32P4_SELECTS_REV_LESS_V3"])
            with self.assertRaises(ValueError):
                packager.validate_idf_profile(build, "rev3_x")

    def test_arduino_requires_one_unambiguous_flash_layout_and_safe_fqbn(self) -> None:
        fqbn = packager.ARDUINO_FQBN
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project = root / "examples" / "arduino" / "HelloWorld"; build = root / "build"; project.mkdir(parents=True); build.mkdir()
            for name in ("HelloWorld.ino.merged.bin", "another-merged.bin"): (build / name).write_bytes(b"x")
            old = Path.cwd(); os.chdir(root)
            try:
                with self.assertRaises(ValueError): packager.package_arduino(project, build, "3.3.11", root / "bad.zip", fqbn, "rev1_3")
                with self.assertRaises(ValueError): packager.package_arduino(project, build, "3.3.11", root / "bad2.zip", "FlashSize=16M", "rev1_3")
                with self.assertRaises(ValueError): packager.parse_arduino_fqbn("esp32:esp32:esp32p4:FlashSize=32M,FlashSize=32M,ChipVariant=prev3,EraseFlash=none")
                with self.assertRaises(ValueError): packager.package_arduino(project, build, "3.3.11", root / "bad3.zip", fqbn, "rev3_x")
            finally:
                os.chdir(old)

    def test_arduino_merged_bundle_has_exact_fqbn_and_offset_zero(self) -> None:
        fqbn = packager.ARDUINO_FQBN
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project = root / "examples" / "arduino" / "HelloWorld"; build = root / "build"; project.mkdir(parents=True); build.mkdir()
            (build / "HelloWorld.ino.merged.bin").write_bytes(b"merged")
            old = Path.cwd(); os.chdir(root)
            try:
                output = packager.package_arduino(project, build, "3.3.11", root / "arduino.zip", fqbn, "rev1_3")
            finally:
                os.chdir(old)
            with zipfile.ZipFile(output) as archive:
                document = json.loads(archive.read("manifest.json"))
            self.assertEqual(fqbn, document["fqbn"])
            self.assertEqual("rev1_3", document["board_profile"])
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
        self.assertIn(packager.ARDUINO_FQBN, arduino_workflow)
        flasher = (ROOT / "scripts" / "ci_firmware.py").read_text(encoding="utf-8")
        self.assertIn("Hash of data verified", flasher)
        self.assertIn("c6_firmware_included", flasher)
        self.assertIn("chip_id", flasher)
        self.assertIn("safe_extract", flasher)
        self.assertNotIn("erase_flash", flasher)

    def test_list_only_matches_selector_inventory_and_artifact_contract(self) -> None:
        import subprocess

        expected = self.expected_list_only_items()
        self.assertEqual(33, len(expected))
        self.assert_static_list_only_contract(expected)

    def test_list_only_static_contract_is_available_without_powershell(self) -> None:
        with mock.patch("shutil.which", return_value=None):
            self.assertIsNone(self.list_only_powershell())
        self.assert_static_list_only_contract(self.expected_list_only_items())


class CiFirmwareCoreTests(unittest.TestCase):
    class FakeApi:
        def __init__(self, runs: dict[str, list[dict[str, object]]], artifacts: dict[int, list[dict[str, object]]]) -> None:
            self.runs, self.artifacts = runs, artifacts

        def get_all(self, endpoint: str) -> list[dict[str, object]]:
            if endpoint.endswith("/artifacts"):
                return self.artifacts[int(endpoint.split("/runs/")[1].split("/")[0])]
            for workflow, runs in self.runs.items():
                if f"/{workflow}/runs" in endpoint:
                    return runs
            raise AssertionError(endpoint)

    def repo(self) -> core.Repository:
        return core.Repository(ROOT, "waveshareteam", ROOT.name, "branch", "a" * 40, True)

    def test_origin_and_dynamic_inventory(self) -> None:
        self.assertEqual(("owner", "repo"), core.parse_github_origin("git@github.com:owner/repo.git"))
        self.assertEqual(("owner", "repo"), core.parse_github_origin("https://github.com/owner/repo.git"))
        with self.assertRaises(core.CiFirmwareError): core.parse_github_origin("https://gitlab.com/owner/repo")
        items = core.expected_items(ROOT)
        self.assertEqual(33, len(items))
        self.assertEqual(26, sum(item.workflow == "esp-idf.yml" for item in items))
        self.assertEqual(5, sum(item.workflow == "arduino.yml" for item in items))
        self.assertEqual(2, sum(item.workflow == "firmware.yml" for item in items))

    def test_select_runs_never_falls_back_from_newer_partial_exact_sha(self) -> None:
        items = (core.Item(1, "test.yml", "one", "esp-idf", "v", "p", "rev1_3"), core.Item(2, "test.yml", "two", "esp-idf", "v", "p", "rev1_3"))
        runs = {"test.yml": [
            {"id": 3, "created_at": "2026-08-12T12:00:00Z", "status": "completed", "conclusion": "success", "head_sha": "a" * 40, "html_url": "new-partial"},
            {"id": 2, "created_at": "2026-08-12T11:00:00Z", "status": "completed", "conclusion": "success", "head_sha": "a" * 40, "html_url": "complete"},
        ]}
        artifacts = {3: [{"name": "one", "size_in_bytes": 1, "expired": False}], 2: [{"name": "one", "size_in_bytes": 1, "expired": False}, {"name": "two", "size_in_bytes": 1, "expired": False}]}
        with self.assertRaises(core.CiFirmwareError): core.select_runs(self.FakeApi(runs, artifacts), self.repo(), items)
        newer_complete = {"test.yml": [
            {"id": 4, "run_started_at": "2026-08-12T13:00:00Z", "status": "completed", "conclusion": "success", "head_sha": "a" * 40, "html_url": "new-complete"},
            *runs["test.yml"],
        ]}
        artifacts[4] = artifacts[2]
        self.assertEqual(4, core.select_runs(self.FakeApi(newer_complete, artifacts), self.repo(), items)["test.yml"].run_id)
        old = {"test.yml": [{"id": 1, "created_at": "2026-08-12T14:00:00Z", "status": "completed", "conclusion": "success", "head_sha": "b" * 40, "html_url": "old"}]}
        with self.assertRaises(core.CiFirmwareError): core.select_runs(self.FakeApi(old, {1: artifacts[2]}), self.repo(), items)

    def test_metadata_item_and_probe_parsers(self) -> None:
        item = core.Item(1, "x", "a", "f", "v", "p", "rev1_3")
        run = core.RunSelection("x", 1, "", "a" * 40, ({"name": "a", "size_in_bytes": 1, "expired": False},))
        self.assertEqual("a", core.artifact_for(run, item)["name"])
        with self.assertRaises(core.CiFirmwareError): core.artifact_for(core.RunSelection("x", 1, "", "", ({"name": "a", "size_in_bytes": 0, "expired": False},)), item)
        self.assertEqual(item, core.resolve_item((item,), "1"))
        self.assertEqual("rev3_x", core.profile_for_major(3))
        self.assertEqual(32 * 1024 * 1024, core.parse_flash_size("Detected flash size: 32MB"))
        self.assertEqual(32 * 1024 * 1024, core.parse_flash_size("Flash size: 32MiB"))
        self.assertEqual(16 * 1024 * 1024, core.parse_flash_size("Flash size: 128Mbit"))
        self.assertEqual(2, core.parse_probe("Chip is ESP32-P4 revision 2"))

    def test_token_fallback_and_wrapper_contracts(self) -> None:
        with mock.patch.object(core.GitHubApi, "get_json", return_value={}) as get_json:
            api = core.GitHubApi(token="not-printed", force_rest=True)
        self.assertIsNone(api.gh)
        get_json.assert_called_once_with("/user")
        powershell = (ROOT / "scripts/Flash-CI-Firmware.ps1").read_text(encoding="utf-8")
        shell = (ROOT / "Flash-CI-Firmware.sh").read_text(encoding="utf-8")
        self.assertIn("ci_firmware.py", powershell)
        self.assertNotIn("Windows.Forms", powershell)
        self.assertTrue(shell.startswith("#!/usr/bin/env bash"))
        self.assertIn('exec "$PYTHON" "$ROOT/scripts/ci_firmware.py" "$@"', shell)

    def test_safe_extract_rejects_traversal_and_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index, (name, link) in enumerate((("../bad", False), ("..\\bad", False), ("C:\\bad", False), ("\\\\host\\share", False), ("link", True))):
                archive = root / f"unsafe-{index}.zip"
                with zipfile.ZipFile(archive, "w") as bundle:
                    info = zipfile.ZipInfo(name)
                    if link: info.external_attr = 0o120777 << 16
                    bundle.writestr(info, b"x")
                with self.assertRaises(core.CiFirmwareError): core.safe_extract(archive, root / f"out-{index}")

    def test_clean_preflight_and_repository_change_are_required(self) -> None:
        dirty = core.Repository(ROOT, "owner", "repo", "branch", "a" * 40, False)
        with self.assertRaises(core.CiFirmwareError): core.preflight(dirty, object(), (), need_esptool=False)
        initial = self.repo()
        changed = core.Repository(ROOT, "owner", ROOT.name, "branch", "a" * 40, True)
        with mock.patch.object(core, "repository", return_value=changed):
            with self.assertRaises(core.CiFirmwareError): core.assert_repository_unchanged(initial)

    def test_manifest_rejects_duplicate_json_and_metadata_drift(self) -> None:
        item = core.expected_items(ROOT)[0]
        repo = self.repo()
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory); (package / "manifest.json").write_text('{"schema_version":1,"schema_version":1}', encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError): core.validate_manifest(package, item, repo)
            package.joinpath("bin").mkdir(); binary = package / "bin" / "app.bin"; binary.write_bytes(b"x")
            package.joinpath("metadata").mkdir()
            package.joinpath("metadata/flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "app.bin", "0x10000": "model.bin"}}), encoding="utf-8")
            document = {"schema_version": 1, "board": ROOT.name, "chip": "esp32p4", "board_profile": item.profile, "chip_revision": {"minimum": "1.0", "maximum_exclusive": "3.0"}, "c6_firmware_included": False, "framework": item.framework, "framework_version": item.version, "source_project": item.source_project, "git_sha": repo.head, "flash": {"baud": 460800, "size_bytes": 32 * 1024 * 1024, "command": "python -m esptool --chip esp32p4 --baud 460800 write_flash 0x0 bin/app.bin"}, "files": [{"offset": "0x0", "archive_path": "bin/app.bin", "metadata_path": "app.bin", "size": 1, "sha256": core.sha256(binary)}]}
            package.joinpath("manifest.json").write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError): core.validate_manifest(package, item, repo)

    def test_arduino_layout_requires_exact_fqbn_and_complete_layout(self) -> None:
        item = next(entry for entry in core.expected_items(ROOT) if entry.framework == "arduino-esp32")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = []
            for offset, name in ((0, "bootloader.bin"), (0x8000, "partitions.bin"), (0xe000, "boot_app0.bin"), (0x10000, Path(item.source_project).name + ".ino.bin")):
                path = root / name; path.write_bytes(b"x"); files.append((offset, 1, path, f"bin/{name}"))
            core.validate_arduino_layout(item, {"fqbn": packager.ARDUINO_FQBN}, files, packager)
            with self.assertRaises(core.CiFirmwareError): core.validate_arduino_layout(item, {"fqbn": "wrong"}, files, packager)
            with self.assertRaises(core.CiFirmwareError): core.validate_arduino_layout(item, {"fqbn": packager.ARDUINO_FQBN}, files[:-1], packager)

    def test_manifest_hash_offsets_command_profile_and_c6_gates(self) -> None:
        item = core.expected_items(ROOT)[0]
        repo = core.Repository(ROOT, "waveshareteam", ROOT.name, "branch", "a" * 40, True)
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory); binary = package / "bin" / "app.bin"; binary.parent.mkdir(); binary.write_bytes(b"firmware")
            (package / "metadata").mkdir()
            (package / "metadata" / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "app.bin"}}), encoding="utf-8")
            manifest = {"schema_version": 1, "board": ROOT.name, "chip": "esp32p4", "board_profile": item.profile, "chip_revision": {"minimum": "1.0", "maximum_exclusive": "3.0"}, "c6_firmware_included": False, "framework": item.framework, "framework_version": item.version, "source_project": item.source_project, "git_sha": repo.head, "flash": {"baud": 460800, "size_bytes": 32 * 1024 * 1024, "command": "python -m esptool --chip esp32p4 --baud 460800 write_flash 0x0 bin/app.bin"}, "files": [{"offset": "0x0", "archive_path": "bin/app.bin", "metadata_path": "app.bin", "size": 8, "sha256": core.sha256(binary)}]}
            (package / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual([(0, binary)], core.validate_manifest(package, item, repo))
            manifest["c6_firmware_included"] = True; (package / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError): core.validate_manifest(package, item, repo)


if __name__ == "__main__":
    unittest.main()
