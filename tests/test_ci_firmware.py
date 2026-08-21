from __future__ import annotations

import importlib.util
import hashlib
import json
import os
import shutil
import subprocess
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
    BSP_VERSION = "3.0.1"
    BSP_SHA = "b" * 40
    BSP_TREE_SHA = "c" * 40

    def setUp(self) -> None:
        self.original_repository_resolver = packager._repository_root_from_script
        self.original_package_git_sha = os.environ.get("PACKAGE_GIT_SHA")

    def tearDown(self) -> None:
        packager._repository_root_from_script = self.original_repository_resolver
        if self.original_package_git_sha is None:
            os.environ.pop("PACKAGE_GIT_SHA", None)
        else:
            os.environ["PACKAGE_GIT_SHA"] = self.original_package_git_sha

    @staticmethod
    def git(root: Path, *arguments: str) -> str:
        completed = subprocess.run(
            ["git", "-C", str(root), *arguments],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        return completed.stdout.strip()

    def commit_repository(self, root: Path) -> str:
        if not (root / ".git").is_dir():
            self.git(root, "init", "--quiet")
            self.git(root, "config", "user.name", "Firmware Test")
            self.git(root, "config", "user.email", "firmware-test@example.invalid")
            (root / ".gitignore").write_text(
                "build/\n**/build/\n*.zip\npackage/\nhelper-run/\nfake-bin/\n"
                "captured-helper-args.txt\nrelease-artifacts/\n",
                encoding="utf-8",
            )
        self.git(root, "add", "--all")
        staged = subprocess.run(
            ["git", "-C", str(root), "diff", "--cached", "--quiet"],
            check=False,
        ).returncode
        if staged:
            self.git(root, "commit", "--quiet", "-m", "test fixture")
        return self.activate_repository(root)

    def activate_repository(self, root: Path) -> str:
        resolved = root.resolve()
        head = self.git(resolved, "rev-parse", "HEAD").lower()
        packager._repository_root_from_script = lambda: resolved
        os.environ["PACKAGE_GIT_SHA"] = head
        return head

    @staticmethod
    def expected_list_only_items() -> list[tuple[str, str]]:
        inventory = selector.discover_inventory(ROOT)
        expected = [
            (f"firmware-esp-idf-{Path(project).name}-{version}-rev3_x", project)
            for project in inventory.idf_projects
            for version in ("v5.5.5", "v6.0.2")
        ]
        expected += [
            (f"firmware-arduino-{Path(sketch).name}-3.3.11-rev3_x", sketch)
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
        (build / "app.bin").write_bytes(b"application compiled from /tmp/external-component.c")
        (build / "config").mkdir()
        (build / "config" / "sdkconfig.json").write_text(json.dumps({"ESP32P4_SELECTS_REV_LESS_V3": False, "ESP32P4_REV_MIN_300": True}), encoding="utf-8")
        (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "bootloader.bin", "0x10000": "app.bin"}}), encoding="utf-8")
        (project / "CMakeLists.txt").write_text("# test fixture\n", encoding="utf-8")
        self.commit_repository(root)
        return project, build

    def arduino_fixture(self, root: Path, name: str = "HelloWorld", boot_offset: int = 0x2000) -> tuple[Path, Path]:
        project = root / "examples" / "arduino" / name
        build = root / "build" / name
        project.mkdir(parents=True)
        build.mkdir(parents=True)
        (project / f"{name}.ino").write_text("void setup() {}\nvoid loop() {}\n", encoding="utf-8")
        files = {
            f"{name}.ino.bootloader.bin": b"bootloader",
            f"{name}.ino.partitions.bin": b"partitions",
            "boot_app0.bin": b"ota-data",
            f"{name}.ino.bin": b"application",
        }
        for filename, content in files.items():
            (build / filename).write_bytes(content)
        (build / "build.options.json").write_text(
            json.dumps({
                "fqbn": packager.ARDUINO_FQBN,
                "hardwareFolders": "/home/example/.arduino15/packages/esp32/hardware/esp32/3.3.11",
                "sketchLocation": str(project),
                "otherLibrariesFolders": "/Users/example/Arduino/libraries",
                "customBuildProperties": "C:\\Users\\example\\cache",
            }),
            encoding="utf-8",
        )
        (build / f"{name}.ino.merged.bin").write_bytes(b"not-a-release-artifact")
        (build / "flash_args").write_text(
            "--flash-mode dio --flash-freq 80m --flash-size 32MB\n"
            f"0x{boot_offset:x} {name}.ino.bootloader.bin\n"
            f"0x8000 {name}.ino.partitions.bin\n"
            "0xe000 boot_app0.bin\n"
            f"0x10000 {name}.ino.bin\n",
            encoding="utf-8",
        )
        self.commit_repository(root)
        return project, build

    def test_idf_bundle_has_schema_one_safe_relative_manifest_and_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.fixture(root); output = root / "artifact.zip"
            old = Path.cwd(); os.chdir(root)
            try:
                packager.package_idf(project, build, "v5.5.5", output, "rev3_x")
            finally:
                os.chdir(old)
            with zipfile.ZipFile(output) as archive:
                document = json.loads(archive.read("manifest.json"))
                self.assertIn("metadata/flasher_args.json", archive.namelist())
            self.assertEqual(1, document["schema_version"])
            self.assertEqual("examples/esp-idf/hello_world", document["source_project"])
            self.assertEqual("esp32p4", document["chip"])
            self.assertEqual("rev3_x", document["board_profile"])
            self.assertEqual("3.0", document["chip_revision"]["minimum"])
            self.assertIsNone(document["chip_revision"]["maximum_exclusive"])
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
                with self.assertRaises(ValueError): packager.package_idf(project, build, "v5.5.5", root / "bad.zip", "rev3_x")
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "bootloader.bin"}, "write_flash_args": ["--erase-all"]}), encoding="utf-8")
                with self.assertRaises(ValueError): packager.package_idf(project, build, "v5.5.5", root / "dangerous.zip", "rev3_x")
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

    def test_arduino_requires_real_safe_metadata_fqbn_profile_and_bsp_binding(self) -> None:
        fqbn = packager.ARDUINO_FQBN
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.arduino_fixture(root)
            old = Path.cwd(); os.chdir(root)
            try:
                (build / "flash_args").unlink()
                with self.assertRaises((ValueError, FileNotFoundError)): packager.package_arduino(project, build, "3.3.11", root / "missing.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                _, build = self.arduino_fixture(root, "Second")
                with self.assertRaises(ValueError): packager.package_arduino(root / "examples/arduino/Second", build, "3.3.11", root / "bad-fqbn.zip", "FlashSize=16M", "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                (build / "build.options.json").write_text(json.dumps({"fqbn": "esp32:esp32:esp32s3"}), encoding="utf-8")
                with self.assertRaises(ValueError): packager.package_arduino(root / "examples/arduino/Second", build, "3.3.11", root / "relabeled.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                (build / "build.options.json").write_text(json.dumps({"fqbn": fqbn, "hardwareFolders": "/toolchain/esp32/3.3.11"}), encoding="utf-8")
                (build / "build.options.json").write_text(json.dumps({"fqbn": fqbn, "hardwareFolders": "/toolchain/esp32/3.3.10"}), encoding="utf-8")
                with self.assertRaises(ValueError):
                    packager.package_arduino(root / "examples/arduino/Second", build, "3.3.11", root / "wrong-core.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                (build / "build.options.json").write_text(json.dumps({"fqbn": fqbn, "hardwareFolders": "/toolchain/esp32/3.3.11"}), encoding="utf-8")
                third_project, third_build = self.arduino_fixture(root, "Third")
                (third_build / "build.options.json").unlink()
                with self.assertRaises((ValueError, FileNotFoundError)):
                    packager.package_arduino(third_project, third_build, "3.3.11", root / "missing-options.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                (third_build / "build.options.json").write_text("{not-json", encoding="utf-8")
                with self.assertRaises(ValueError):
                    packager.package_arduino(third_project, third_build, "3.3.11", root / "malformed-options.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                with self.assertRaises(ValueError): packager.parse_arduino_fqbn("esp32:esp32:esp32p4:FlashSize=32M,FlashSize=32M,ChipVariant=prev3,EraseFlash=none")
                with self.assertRaises(ValueError): packager.package_arduino(root / "examples/arduino/Second", build, "3.3.11", root / "bad-profile.zip", fqbn, "rev1_3", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                with self.assertRaises(ValueError): packager.package_arduino(root / "examples/arduino/Second", build, "3.3.11", root / "bad-bsp.zip", fqbn, "rev3_x", self.BSP_VERSION, "short", self.BSP_TREE_SHA)
                with self.assertRaises(ValueError): packager.package_arduino(root / "examples/arduino/Second", build, "3.3.11", root / "bad-tree.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, "short")
            finally:
                os.chdir(old)

    def test_arduino_segment_bundle_comes_only_from_flash_args_with_full_provenance(self) -> None:
        fqbn = packager.ARDUINO_FQBN
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.arduino_fixture(root)
            chunk = b"\0" * (1024 * 1024)
            for size_mib in (16, 32):
                with (build / f"legacy-{size_mib}MiB.merged.bin").open("wb") as merged:
                    for _ in range(size_mib):
                        merged.write(chunk)
            old = Path.cwd(); os.chdir(root)
            try:
                output = packager.package_arduino(project, build, "3.3.11", root / "arduino.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
            finally:
                os.chdir(old)
            trusted_head = self.git(root, "rev-parse", "HEAD").lower()
            with zipfile.ZipFile(output) as archive:
                document = json.loads(archive.read("manifest.json"))
                names = archive.namelist()
                metadata_payload = archive.read("metadata/flash_args")
                canonical_identity_payload = archive.read("metadata/arduino_build_identity.json")
                segment_payloads = {
                    entry["archive_path"]: archive.read(entry["archive_path"])
                    for entry in document["files"]
                }
            self.assertEqual(fqbn, document["fqbn"])
            self.assertEqual("rev3_x", document["board_profile"])
            self.assertEqual(["0x2000", "0x8000", "0xe000", "0x10000"], [entry["offset"] for entry in document["files"]])
            self.assertEqual(["bootloader", "partition_table", "boot_app0", "application"], [entry["role"] for entry in document["files"]])
            self.assertTrue(all(entry["metadata_source"] == "metadata/flash_args" for entry in document["files"]))
            self.assertTrue(all(entry["product_git_sha"] == trusted_head for entry in document["files"]))
            self.assertTrue(all(entry["bsp_source_git_sha"] == self.BSP_SHA for entry in document["files"]))
            self.assertTrue(all(entry["bsp_component_git_tree_sha"] == self.BSP_TREE_SHA for entry in document["files"]))
            self.assertTrue(all(entry["bsp_applicable"] is False for entry in document["files"]))
            self.assertFalse(document["bsp"]["applicable"])
            self.assertEqual(460800, document["flash"]["baud"])
            self.assertEqual(32 * 1024 * 1024, document["flash"]["full_flash_bytes"])
            self.assertEqual(sum(entry["size"] for entry in document["files"]), document["flash"]["segmented_bytes"])
            self.assertEqual(document["flash"]["segmented_bytes"], document["flash"]["segmented_payload_total"])
            self.assertLessEqual(document["flash"]["segmented_payload_total"], document["flash"]["full_flash_bytes"] // 2)
            self.assertEqual(["--flash-mode", "dio", "--flash-freq", "80m", "--flash-size", "32MB"], document["arduino_write_flash_args"])
            self.assertIn("metadata/flash_args", names)
            self.assertEqual(document["arduino_metadata"]["size"], len(metadata_payload))
            self.assertEqual(document["arduino_metadata"]["sha256"], hashlib.sha256(metadata_payload).hexdigest())
            self.assertEqual(document["arduino_build_identity"]["size"], len(canonical_identity_payload))
            self.assertEqual(document["arduino_build_identity"]["sha256"], hashlib.sha256(canonical_identity_payload).hexdigest())
            canonical_identity = json.loads(canonical_identity_payload)
            self.assertEqual("build.options.json", canonical_identity["source_name"])
            self.assertEqual(packager.ARDUINO_FQBN, canonical_identity["fqbn"])
            self.assertEqual("3.3.11", canonical_identity["core_version"])
            self.assertEqual("examples/arduino/HelloWorld", canonical_identity["sketch_path"])
            self.assertEqual("HelloWorld", canonical_identity["sketch_name"])
            self.assertEqual("HelloWorld.ino.bin", canonical_identity["application_metadata_path"])
            self.assertEqual(canonical_identity["source_name"], document["arduino_build_identity"]["source_name"])
            self.assertEqual(canonical_identity["source_size"], document["arduino_build_identity"]["source_size"])
            self.assertEqual(canonical_identity["source_sha256"], document["arduino_build_identity"]["source_sha256"])
            for entry in document["files"]:
                payload = segment_payloads[entry["archive_path"]]
                self.assertEqual(entry["size"], len(payload))
                self.assertEqual(entry["sha256"], hashlib.sha256(payload).hexdigest())
            self.assertNotIn("build.options.json", names)
            self.assertIn("metadata/arduino_build_identity.json", names)
            self.assertFalse(document["arduino_build_identity"]["raw_archived"])
            self.assertFalse(any("merged" in name.casefold() for name in names))
            with zipfile.ZipFile(output) as archive:
                public_payloads = [archive.read(name) for name in archive.namelist()]
            for private_marker in (b"/home/example", b"/tmp/private-work", b"/Users/example", b"C:\\Users\\example"):
                self.assertFalse(any(private_marker in payload for payload in public_payloads))
            self.assertIn("write_flash", document["flash"]["command"])
            self.assertIn("--flash-mode dio --flash-freq 80m --flash-size 32MB", document["flash"]["command"])
            self.assertNotIn("erase", document["flash"]["command"].lower())
            with zipfile.ZipFile(output) as archive:
                shell = archive.read("flash.sh")
                batch = archive.read("flash.cmd")
            self.assertIn(b'--port "$PORT"', shell)
            self.assertIn(b'--port "%PORT%"', batch)
            self.assertNotIn(b'"$@"', shell)
            self.assertNotIn(b"%*", batch)
            self.assertIn(b'if not "%3"=="" goto usage', batch)
            self.assertNotIn(b'if not "%~3"=="" goto usage', batch)
            self.assertIn(b'if not "%PORT:#=%"=="%PORT%" goto usage', batch)
            self.assertIn(b'for /f "eol=# delims=', batch)
            self.assertNotIn(b"eol=;", batch)

            helper_root = root / "helper-run"
            with zipfile.ZipFile(output) as archive:
                archive.extractall(helper_root)
            fake_bin = root / "fake-bin"
            fake_bin.mkdir()
            fake_python = fake_bin / "python"
            capture = root / "captured-helper-args.txt"
            fake_python.write_text(
                "#!/usr/bin/env sh\nprintf '%s\\n' \"$@\" > \"$CAPTURE\"\n",
                encoding="utf-8",
            )
            fake_python.chmod(0o755)
            helper_environment = dict(os.environ)
            helper_environment.update({
                "PATH": f"{fake_bin}{os.pathsep}{helper_environment.get('PATH', '')}",
                "CAPTURE": str(capture),
            })
            shell_path = helper_root / "flash.sh"
            for arguments in ((), ("--port", "--chip"), ("--port", "COM17", "--erase-all")):
                completed = subprocess.run(
                    ["sh", str(shell_path), *arguments],
                    cwd=helper_root,
                    env=helper_environment,
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertEqual(2, completed.returncode, arguments)
            completed = subprocess.run(
                ["sh", str(shell_path), "--port", "COM17"],
                cwd=helper_root,
                env=helper_environment,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(0, completed.returncode, completed.stderr)
            captured = capture.read_text(encoding="utf-8").splitlines()
            self.assertEqual(
                ["-m", "esptool", "--port", "COM17", "--chip", "esp32p4", "--baud", "460800", "write_flash"],
                captured[:9],
            )
            self.assertEqual(document["arduino_write_flash_args"], captured[9:15])

    def test_arduino_build_identity_rejects_swapped_or_relabeled_sketches(self) -> None:
        fqbn = packager.ARDUINO_FQBN
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            project_a, build_a = self.arduino_fixture(root, "Alpha")
            project_b, build_b = self.arduino_fixture(root, "Beta")
            old = Path.cwd(); os.chdir(root)
            try:
                with self.assertRaises(ValueError):
                    packager.package_arduino(
                        project_a, build_b, "3.3.11", root / "swapped.zip", fqbn,
                        "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA,
                    )

                impersonator = root / "other" / "examples" / "arduino" / "Alpha"
                impersonator.mkdir(parents=True)
                (impersonator / "Alpha.ino").write_text("void setup() {}\n", encoding="utf-8")
                options_path = build_a / "build.options.json"
                options = json.loads(options_path.read_text(encoding="utf-8"))
                options["sketchLocation"] = str(impersonator)
                options_path.write_text(json.dumps(options), encoding="utf-8")
                with self.assertRaises(ValueError):
                    packager.package_arduino(
                        project_a, build_a, "3.3.11", root / "impersonated.zip", fqbn,
                        "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA,
                    )

                options["sketchLocation"] = str(project_a)
                options_path.write_text(json.dumps(options), encoding="utf-8")
                (build_a / "Relabeled.ino.bin").write_bytes(b"application")
                (build_a / "flash_args").write_text(
                    "--flash-mode dio --flash-freq 80m --flash-size 32MB\n"
                    "0x2000 Alpha.ino.bootloader.bin\n0x8000 Alpha.ino.partitions.bin\n"
                    "0xe000 boot_app0.bin\n0x10000 Relabeled.ino.bin\n",
                    encoding="utf-8",
                )
                with self.assertRaises(ValueError):
                    packager.package_arduino(
                        project_a, build_a, "3.3.11", root / "relabeled.zip", fqbn,
                        "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA,
                    )
            finally:
                os.chdir(old)

    def test_arduino_download_rejects_coordinated_source_project_resigning(self) -> None:
        fqbn = packager.ARDUINO_FQBN
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            project, build = self.arduino_fixture(root)
            self.arduino_fixture(root, "Drawing_board")
            old = Path.cwd(); os.chdir(root)
            try:
                output = packager.package_arduino(
                    project, build, "3.3.11", root / "base.zip", fqbn,
                    "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA,
                )
            finally:
                os.chdir(old)
            trusted_root, trusted_head = packager.trusted_repository()
            with zipfile.ZipFile(output) as archive:
                members = {name: archive.read(name) for name in archive.namelist()}
                document = json.loads(members["manifest.json"])
                canonical = json.loads(members["metadata/arduino_build_identity.json"])
            extras = ["metadata/flash_args", "metadata/arduino_build_identity.json"]

            def resign(target_project: str, sketch_name: str, primary: dict[str, object]) -> tuple[Path, dict[str, object]]:
                changed = json.loads(json.dumps(document))
                changed["source_project"] = target_project
                changed_canonical = json.loads(json.dumps(canonical))
                changed_canonical["sketch_path"] = target_project
                changed_canonical["sketch_name"] = sketch_name
                changed_canonical["primary_source"] = primary
                payload = (json.dumps(changed_canonical, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
                changed["arduino_build_identity"]["size"] = len(payload)
                changed["arduino_build_identity"]["sha256"] = hashlib.sha256(payload).hexdigest()
                changed_members = dict(members)
                changed_members["manifest.json"] = (json.dumps(changed, indent=2, sort_keys=True) + "\n").encode("utf-8")
                changed_members["metadata/arduino_build_identity.json"] = payload
                target = root / f"resigned-{sketch_name}.zip"
                with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                    for name, payload in changed_members.items():
                        archive.writestr(name, payload)
                return target, changed

            # Coordinated re-sign to a non-existent project path.
            ghost = {"path": "examples/arduino/Ghost/Ghost.ino", "size": 32, "sha256": "e" * 64, "git_blob_sha": "f" * 40}
            ghost_zip, ghost_doc = resign("examples/arduino/Ghost", "Ghost", ghost)
            with self.assertRaises(ValueError):
                packager.validate_written_bundle(ghost_zip, ghost_doc, extras, trusted_head, trusted_root)

            # Coordinated re-sign to another real project, keeping a stale primary identity.
            stale = dict(canonical["primary_source"])
            board_zip, board_doc = resign("examples/arduino/Drawing_board", "Drawing_board", stale)
            with self.assertRaises(ValueError):
                packager.validate_written_bundle(board_zip, board_doc, extras, trusted_head, trusted_root)

    def test_arduino_download_rejects_cross_checkout_package(self) -> None:
        fqbn = packager.ARDUINO_FQBN
        with tempfile.TemporaryDirectory() as dir_a, tempfile.TemporaryDirectory() as dir_b:
            root_a, root_b = Path(dir_a), Path(dir_b)
            project_a, build_a = self.arduino_fixture(root_a)
            old = Path.cwd(); os.chdir(root_a)
            try:
                output = packager.package_arduino(
                    project_a, build_a, "3.3.11", root_a / "base.zip", fqbn,
                    "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA,
                )
            finally:
                os.chdir(old)
            # A second checkout with the same layout but a different tracked sketch.
            self.arduino_fixture(root_b)
            (root_b / "examples" / "arduino" / "HelloWorld" / "HelloWorld.ino").write_text(
                "void setup() {}\nvoid loop() {}\n// checkout B\n", encoding="utf-8"
            )
            self.commit_repository(root_b)
            head_b = self.git(root_b, "rev-parse", "HEAD").lower()
            package = root_a / "extracted"
            with zipfile.ZipFile(output) as archive:
                archive.extractall(package)
            packager._repository_root_from_script = lambda: root_b.resolve()
            with self.assertRaises(ValueError):
                packager.validate_arduino_extracted_bundle(package, head_b)

    def test_private_path_scanner_rejects_drive_and_unc_roots_but_allows_upstream_paths(self) -> None:
        for payload in (
            b"/home/ubuntu",
            b"/Users/alice",
            b"/tmp",
            b"/private/tmp",
            b"/root/project",
            b"/workspace/project",
            b"/workspaces/project",
            b"C:\\work\\artifact.bin",
            b"d:/cache/tool.bin",
            b"\\\\server\\share",
            b"prefix \\\\server/share/folder suffix",
            b"//server/share",
            b"prefix //server/share/folder suffix",
        ):
            with self.assertRaises(ValueError, msg=payload):
                packager.reject_private_artifact_bytes(payload, "fixture")
        for payload in (
            b"/IDF/components/freertos/file.c",
            b"/builds/idf/crosstool-NG/source.c",
            b"/dev/ttyUSB0",
            b"C:relative-file.txt",
        ):
            packager.reject_private_artifact_bytes(payload, "fixture")

    def test_arduino_parser_rejects_merged_unsafe_symlink_overlap_and_range_but_allows_bootloader_zero(self) -> None:
        fqbn = packager.ARDUINO_FQBN
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.arduino_fixture(root, boot_offset=0)
            old = Path.cwd(); os.chdir(root)
            try:
                output = packager.package_arduino(project, build, "3.3.11", root / "zero.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                self.assertTrue(output.is_file())
                cases = {
                    "merged": "--flash-mode dio --flash-freq 80m --flash-size 32MB\n0x0 HelloWorld.ino.merged.bin\n",
                    "escape": "--flash-mode dio --flash-freq 80m --flash-size 32MB\n0x0 ../outside.bin\n",
                    "drive": "--flash-mode dio --flash-freq 80m --flash-size 32MB\n0x0 C:/outside.bin\n",
                    "unc": "--flash-mode dio --flash-freq 80m --flash-size 32MB\n0x0 //server/share.bin\n",
                    "danger": "--flash-mode dio --flash-freq 80m --flash-size 32MB --erase-all\n0x0 HelloWorld.ino.bootloader.bin\n",
                }
                for label, text in cases.items():
                    (build / "flash_args").write_text(text, encoding="utf-8")
                    with self.assertRaises(ValueError, msg=label):
                        packager.package_arduino(project, build, "3.3.11", root / f"{label}.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                self.arduino_fixture(root, "Overlap")
                overlap_project = root / "examples/arduino/Overlap"; overlap_build = root / "build/Overlap"
                (overlap_build / "flash_args").write_text(
                    "--flash-mode dio --flash-freq 80m --flash-size 32MB\n"
                    "0x2000 Overlap.ino.bootloader.bin\n0x2004 Overlap.ino.partitions.bin\n"
                    "0xe000 boot_app0.bin\n0x10000 Overlap.ino.bin\n", encoding="utf-8")
                with self.assertRaises(ValueError):
                    packager.package_arduino(overlap_project, overlap_build, "3.3.11", root / "overlap.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                self.arduino_fixture(root, "Range", boot_offset=0x1FFFFFF)
                with self.assertRaises(ValueError):
                    packager.package_arduino(root / "examples/arduino/Range", root / "build/Range", "3.3.11", root / "range.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                self.arduino_fixture(root, "Linked")
                linked_project = root / "examples/arduino/Linked"; linked_build = root / "build/Linked"
                target = linked_build / "real.bin"; target.write_bytes(b"boot")
                (linked_build / "Linked.ino.bootloader.bin").unlink()
                (linked_build / "Linked.ino.bootloader.bin").symlink_to(target)
                with self.assertRaises(ValueError):
                    packager.package_arduino(linked_project, linked_build, "3.3.11", root / "linked.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                # Opaque compiled images are exempt from the text privacy scan, so a
                # sanitized //IDF path inside a .bin payload still packages cleanly.
                self.arduino_fixture(root, "Opaque")
                (root / "build/Opaque/Opaque.ino.bin").write_bytes(b"//IDF/components/sanitized.c")
                opaque_output = packager.package_arduino(root / "examples/arduino/Opaque", root / "build/Opaque", "3.3.11", root / "opaque.zip", fqbn, "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA)
                self.assertTrue(opaque_output.is_file())
            finally:
                os.chdir(old)

    def test_arduino_zip_readback_rejects_tampered_metadata_segment_totals_and_extra_raw_properties(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project, build = self.arduino_fixture(root)
            old = Path.cwd(); os.chdir(root)
            try:
                output = packager.package_arduino(
                    project, build, "3.3.11", root / "base.zip", packager.ARDUINO_FQBN,
                    "rev3_x", self.BSP_VERSION, self.BSP_SHA, self.BSP_TREE_SHA,
                )
            finally:
                os.chdir(old)
            with zipfile.ZipFile(output) as archive:
                base_members = {name: archive.read(name) for name in archive.namelist()}
                document = json.loads(base_members["manifest.json"])

            trusted_root, trusted_head = packager.trusted_repository()
            extras = ["metadata/flash_args", "metadata/arduino_build_identity.json"]

            def tampered(
                name: str,
                replacements: dict[str, bytes],
                added: dict[str, bytes] | None = None,
                deleted: set[str] | None = None,
            ) -> Path:
                target = root / name
                with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                    for member_name, payload in base_members.items():
                        if member_name in (deleted or set()):
                            continue
                        archive.writestr(member_name, replacements.get(member_name, payload))
                    for member_name, payload in (added or {}).items():
                        archive.writestr(member_name, payload)
                return target

            segment_name = document["files"][0]["archive_path"]
            cases = (
                ("flash-args.zip", {"metadata/flash_args": base_members["metadata/flash_args"] + b"\n"}, None, document),
                ("canonical.zip", {"metadata/arduino_build_identity.json": b"{}\n"}, None, document),
                ("segment.zip", {segment_name: base_members[segment_name] + b"x"}, None, document),
                ("raw.zip", {}, {"metadata/build.options.json": b"/home/example/private"}, document),
            )
            for name, replacements, added, contract in cases:
                with self.assertRaises(ValueError, msg=name):
                    packager.validate_written_bundle(tampered(name, replacements, added), contract, extras, trusted_head, trusted_root)

            last = document["files"][-1]
            helper_contracts = {
                "flash.sh": {
                    "segment-delete": f'{last["offset"]} "$DIR/{last["archive_path"]}"'.encode(),
                    "offset": str(last["offset"]).encode(),
                    "filename": str(last["archive_path"]).encode(),
                    "option": b"--flash-mode dio",
                    "erase": b"write_flash",
                },
                "flash.cmd": {
                    "segment-delete": (
                        f'{last["offset"]} "%~dp0{str(last["archive_path"]).replace("/", chr(92))}"'
                    ).encode(),
                    "offset": str(last["offset"]).encode(),
                    "filename": str(last["archive_path"]).replace("/", "\\").encode(),
                    "option": b"--flash-mode dio",
                    "erase": b"write_flash",
                },
            }
            replacements_by_case = {
                "segment-delete": b"",
                "offset": b"0x11000",
                "filename": b"bin/renamed.bin",
                "option": b"--flash-mode qio",
                "erase": b"erase_flash",
            }
            for helper_name, mutations in helper_contracts.items():
                with self.assertRaises(ValueError, msg=f"delete-{helper_name}"):
                    packager.validate_written_bundle(
                        tampered(f"delete-{helper_name}.zip", {}, deleted={helper_name}),
                        document,
                        extras,
                        trusted_head,
                        trusted_root,
                    )
                for label, original in mutations.items():
                    self.assertIn(original, base_members[helper_name], (helper_name, label))
                    changed_helper = base_members[helper_name].replace(
                        original, replacements_by_case[label], 1
                    )
                    with self.assertRaises(ValueError, msg=f"{helper_name}-{label}"):
                        packager.validate_written_bundle(
                            tampered(
                                f"{helper_name.replace('.', '-')}-{label}.zip",
                                {helper_name: changed_helper},
                            ),
                            document,
                            extras,
                            trusted_head,
                            trusted_root,
                        )

            changed = json.loads(json.dumps(document))
            changed["flash"]["segmented_payload_total"] += 1
            changed_manifest = (json.dumps(changed, indent=2, sort_keys=True) + "\n").encode("utf-8")
            with self.assertRaises(ValueError):
                packager.validate_written_bundle(
                    tampered("totals.zip", {"manifest.json": changed_manifest}), changed, extras, trusted_head, trusted_root
                )

            manifest_mutations = []
            changed = json.loads(json.dumps(document)); changed["fqbn"] = "esp32:esp32:esp32p4:wrong"
            manifest_mutations.append(("top-fqbn", changed))
            changed = json.loads(json.dumps(document)); changed["bsp"]["source_git_sha"] = "d" * 40
            manifest_mutations.append(("bsp-source", changed))
            changed = json.loads(json.dumps(document)); changed["flash"]["command"] = "python -m esptool write_flash arbitrary.bin"
            manifest_mutations.append(("command", changed))
            changed = json.loads(json.dumps(document)); changed["files"][1]["offset"] = changed["files"][0]["offset"]
            manifest_mutations.append(("overlap", changed))
            changed = json.loads(json.dumps(document)); changed["files"][0]["metadata_path"] = "other.bin"
            manifest_mutations.append(("metadata-drift", changed))
            changed = json.loads(json.dumps(document)); changed["schema_version"] = 2
            manifest_mutations.append(("schema", changed))
            changed = json.loads(json.dumps(document)); changed["c6_firmware_included"] = True
            manifest_mutations.append(("c6", changed))
            changed = json.loads(json.dumps(document)); changed["chip_revision"]["minimum"] = "1.0"
            manifest_mutations.append(("revision", changed))
            changed = json.loads(json.dumps(document)); changed["generated_at_utc"] = "not-a-timestamp"
            manifest_mutations.append(("generated-at", changed))
            changed = json.loads(json.dumps(document)); changed["unexpected"] = "semantic-field"
            manifest_mutations.append(("top-extra", changed))
            changed = json.loads(json.dumps(document)); changed["flash"]["unexpected"] = 1
            manifest_mutations.append(("flash-extra", changed))
            changed = json.loads(json.dumps(document)); changed["bsp"]["unexpected"] = 1
            manifest_mutations.append(("bsp-extra", changed))
            for label, changed in manifest_mutations:
                payload = (json.dumps(changed, indent=2, sort_keys=True) + "\n").encode("utf-8")
                with self.assertRaises(ValueError, msg=label):
                    packager.validate_written_bundle(
                        tampered(f"manifest-{label}.zip", {"manifest.json": payload}),
                        changed,
                        extras,
                        trusted_head,
                        trusted_root,
                    )

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

    def test_local_packaging_also_rejects_missing_git_sha(self) -> None:
        with mock.patch.dict(os.environ, {"GITHUB_SHA": "d" * 40}, clear=True):
            with self.assertRaises(ValueError):
                packager.git_sha()

    def test_workflow_and_windows_safety_contract(self) -> None:
        for relative in (".github/workflows/esp-idf.yml", ".github/workflows/arduino.yml", ".github/workflows/firmware.yml"):
            text = (ROOT / relative).read_text(encoding="utf-8")
            self.assertIn("PACKAGE_GIT_SHA: ${{ github.event.pull_request.head.sha || github.sha }}", text)
            self.assertIn("actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a", text)
        arduino_workflow = (ROOT / ".github/workflows/arduino.yml").read_text(encoding="utf-8")
        self.assertIn("--build-path", arduino_workflow)
        self.assertNotIn("--output-dir", arduino_workflow)
        self.assertNotIn("--export-binaries", arduino_workflow)
        self.assertIn("--bsp-source-git-sha", arduino_workflow)
        self.assertIn("--bsp-component-tree-sha", arduino_workflow)
        self.assertIn('WAVESHARE_BSP_VERSION: "3.0.1"', arduino_workflow)
        self.assertIn('WAVESHARE_BSP_SOURCE_GIT_SHA: "69b3e7ba512e3676519196f5d91680445600a101"', arduino_workflow)
        self.assertIn('WAVESHARE_BSP_COMPONENT_TREE_SHA: "cbab0682683616cb6cb1a4efc5c6641676bb5b59"', arduino_workflow)
        for required in ("compiler.c.extra_flags=", "compiler.cpp.extra_flags=", "compiler.S.extra_flags=", "-ffile-prefix-map=", "-fmacro-prefix-map="):
            self.assertIn(required, arduino_workflow)
        self.assertNotIn("build.extra_flags=", arduino_workflow)
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
        self.assertEqual(38, len(expected))
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

    def trusted_repo(self) -> core.Repository:
        root, head = packager.trusted_repository()
        return core.Repository(root, "waveshareteam", root.name, "test", head, True)

    def test_origin_and_dynamic_inventory(self) -> None:
        self.assertEqual(("owner", "repo"), core.parse_github_origin("git@github.com:owner/repo.git"))
        self.assertEqual(("owner", "repo"), core.parse_github_origin("https://github.com/owner/repo.git"))
        with self.assertRaises(core.CiFirmwareError): core.parse_github_origin("https://gitlab.com/owner/repo")
        items = core.expected_items(ROOT)
        self.assertEqual(38, len(items))
        self.assertEqual(26, sum(item.workflow == "esp-idf.yml" for item in items))
        self.assertEqual(10, sum(item.workflow == "arduino.yml" for item in items))
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

    def test_arduino_layout_requires_exact_packaged_metadata_and_provenance(self) -> None:
        item = next(entry for entry in core.expected_items(ROOT) if entry.framework == "arduino-esp32")
        repo = self.trusted_repo()
        project = ROOT / item.source_project
        name = project.name
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            for filename in (f"{name}.ino.bootloader.bin", f"{name}.ino.partitions.bin", "boot_app0.bin", f"{name}.ino.bin"):
                (build / filename).write_bytes(filename.encode("utf-8"))
            (build / "build.options.json").write_text(
                json.dumps({"fqbn": packager.ARDUINO_FQBN, "hardwareFolders": "/home/example/.arduino15/packages/esp32/hardware/esp32/3.3.11", "sketchLocation": str(project)}),
                encoding="utf-8",
            )
            (build / "flash_args").write_text(
                "--flash-mode dio --flash-freq 80m --flash-size 32MB\n"
                f"0x2000 {name}.ino.bootloader.bin\n0x8000 {name}.ino.partitions.bin\n"
                f"0xe000 boot_app0.bin\n0x10000 {name}.ino.bin\n", encoding="utf-8")
            old = Path.cwd(); os.chdir(ROOT)
            try:
                with mock.patch.dict(os.environ, {"PACKAGE_GIT_SHA": repo.head}):
                    package_zip = packager.package_arduino(
                        project, build, item.version, root / "package.zip", packager.ARDUINO_FQBN,
                        item.profile, "3.0.1", "b" * 40, "c" * 40,
                    )
            finally:
                os.chdir(old)
            package = root / "package"
            with zipfile.ZipFile(package_zip) as archive:
                archive.extractall(package)
            plan, options = core.validate_manifest(package, item, repo)
            self.assertEqual([0x2000, 0x8000, 0xe000, 0x10000], [offset for offset, _ in plan])
            self.assertEqual(["--flash-mode", "dio", "--flash-freq", "80m", "--flash-size", "32MB"], options)
            manifest_path = package / "manifest.json"
            document = json.loads(manifest_path.read_text(encoding="utf-8"))
            for key, bad_value in (("fqbn", "wrong"), ("target", "esp32s3")):
                changed = json.loads(json.dumps(document)); changed[key] = bad_value
                manifest_path.write_text(json.dumps(changed), encoding="utf-8")
                with self.assertRaises(core.CiFirmwareError, msg=key):
                    core.validate_manifest(package, item, repo)
            changed = json.loads(json.dumps(document)); changed["files"][0]["bsp_component_git_tree_sha"] = "d" * 40
            manifest_path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)
            changed = json.loads(json.dumps(document)); changed["flash"]["segmented_payload_total"] += 1
            manifest_path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)
            changed = json.loads(json.dumps(document)); changed["arduino_build_identity"]["source_size"] += 1
            manifest_path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)
            changed = json.loads(json.dumps(document)); changed["privacy_leak"] = "/home/example/private"
            manifest_path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)
            manifest_path.write_text(json.dumps(document), encoding="utf-8")
            canonical_path = package / "metadata" / "arduino_build_identity.json"
            original_canonical = canonical_path.read_bytes()
            canonical_path.write_text(json.dumps({"fqbn": packager.ARDUINO_FQBN, "leak": "/tmp/private"}), encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)
            canonical_path.write_bytes(original_canonical)
            raw_options = package / "build.options.json"
            raw_options.write_text('{"sketchLocation":"/home/example/private"}', encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)
            raw_options.unlink()
            expanded = package / "metadata" / "expanded-properties.txt"
            expanded.write_text("/Users/example/cache", encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)
            expanded.unlink()
            segment_path = plan[0][1]
            original_segment = segment_path.read_bytes()
            segment_path.write_bytes(original_segment + b"x")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)
            segment_path.write_bytes(original_segment)
            metadata_path = package / "metadata" / "flash_args"
            metadata_path.write_text(metadata_path.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError):
                core.validate_manifest(package, item, repo)

    def test_manifest_hash_offsets_command_profile_and_c6_gates(self) -> None:
        item = core.expected_items(ROOT)[0]
        repo = self.trusted_repo()
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory); binary = package / "bin" / "app.bin"; binary.parent.mkdir(); binary.write_bytes(b"firmware")
            (package / "metadata").mkdir()
            (package / "metadata" / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "app.bin"}}), encoding="utf-8")
            profile = packager.BOARD_PROFILES[item.profile]
            manifest = {"schema_version": 1, "board": ROOT.name, "chip": "esp32p4", "board_profile": item.profile, "chip_revision": {"minimum": profile["minimum"], "maximum_exclusive": profile["maximum_exclusive"]}, "c6_firmware_included": False, "framework": item.framework, "framework_version": item.version, "source_project": item.source_project, "git_sha": repo.head, "flash": {"baud": 460800, "size_bytes": 32 * 1024 * 1024, "command": "python -m esptool --chip esp32p4 --baud 460800 write_flash 0x0 bin/app.bin"}, "files": [{"offset": "0x0", "archive_path": "bin/app.bin", "metadata_path": "app.bin", "size": 8, "sha256": core.sha256(binary)}]}
            (package / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(([(0, binary)], []), core.validate_manifest(package, item, repo))
            manifest["c6_firmware_included"] = True; (package / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaises(core.CiFirmwareError): core.validate_manifest(package, item, repo)


if __name__ == "__main__":
    unittest.main()
