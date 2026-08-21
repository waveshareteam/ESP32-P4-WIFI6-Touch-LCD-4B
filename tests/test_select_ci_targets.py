from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "select_ci_targets.py"
SPEC = importlib.util.spec_from_file_location("select_ci_targets", SCRIPT)
assert SPEC and SPEC.loader
selector = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = selector
SPEC.loader.exec_module(selector)


class SelectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.inventory = selector.discover_inventory(ROOT)

    def route(self, framework: str, *paths: str):
        return selector.select_from_changes(framework, self.inventory, paths)

    def test_repository_inventory_has_expected_first_party_targets(self) -> None:
        self.assertEqual(13, len(self.inventory.idf_projects))
        self.assertEqual(10, len(self.inventory.arduino_sketches))
        self.assertNotIn("firmware/brookesia", self.inventory.idf_projects)
        self.assertFalse(any("/libraries/" in path for path in self.inventory.arduino_sketches))
        self.assertIn("examples/arduino/examples/10_Mic_Record", self.inventory.arduino_sketches)

    def test_documentation_and_governance_do_not_build(self) -> None:
        paths = (
            "README.md",
            "docs/ci.md",
            "examples/esp-idf/hello_world/README.md",
            "examples/arduino/examples/01_HelloWorld/README.md",
            "examples/arduino/libraries/displays/README.md",
            ".github/ISSUE_TEMPLATE/bug_report.md",
        )
        self.assertFalse(self.route("esp-idf", *paths).builds)
        self.assertFalse(self.route("arduino", *paths).builds)

    def test_direct_example_source_routes_only_that_example(self) -> None:
        idf = self.route("esp-idf", "examples/esp-idf/hello_world/main/hello_world_main.c")
        arduino = self.route("arduino", "examples/arduino/examples/01_HelloWorld/01_HelloWorld.ino")
        self.assertEqual(("examples/esp-idf/hello_world",), idf.selected)
        self.assertEqual(2, len(idf.builds))
        self.assertTrue(all("@sha256:" in build["idf_image"] for build in idf.builds))
        self.assertEqual(("examples/arduino/examples/01_HelloWorld",), arduino.selected)
        self.assertEqual(1, len(arduino.builds))

    def test_shared_inputs_route_complete_affected_framework(self) -> None:
        idf = self.route("esp-idf", "config/sdkconfig.defaults")
        arduino = self.route(
            "arduino",
            "examples/arduino/libraries/displays/displays_config.h",
        )
        self.assertEqual(26, len(idf.builds))
        self.assertEqual(10, len(arduino.builds))

    def test_workflow_changes_route_only_its_framework(self) -> None:
        self.assertEqual(26, len(self.route("esp-idf", ".github/workflows/esp-idf.yml").builds))
        self.assertFalse(self.route("arduino", ".github/workflows/esp-idf.yml").builds)
        self.assertEqual(10, len(self.route("arduino", ".github/workflows/arduino.yml").builds))
        self.assertFalse(self.route("esp-idf", ".github/workflows/arduino.yml").builds)

    def test_routing_helpers_and_their_tests_route_both_frameworks(self) -> None:
        for path in (
            "scripts/collect_ci_changes.py",
            "scripts/select_ci_targets.py",
            "tests/test_collect_ci_changes.py",
            "tests/test_select_ci_targets.py",
        ):
            self.assertEqual(26, len(self.route("esp-idf", path).builds), path)
            self.assertEqual(10, len(self.route("arduino", path).builds), path)

    def test_ci_flasher_packager_and_tests_are_global_build_inputs(self) -> None:
        for path in (
            "Flash-CI-Firmware.cmd",
            "Flash-CI-Firmware.sh",
            "scripts/Flash-CI-Firmware.ps1",
            "scripts/ci_firmware.py",
            "scripts/package_ci_firmware.py",
            "scripts/check_repository_policy.py",
            "tests/test_ci_firmware.py",
        ):
            self.assertEqual(26, len(self.route("esp-idf", path).builds), path)
            self.assertEqual(10, len(self.route("arduino", path).builds), path)

    def test_matrix_uses_safe_immediate_target_names(self) -> None:
        idf = self.route("esp-idf", "examples/esp-idf/hello_world/main/hello_world_main.c")
        arduino = self.route("arduino", "examples/arduino/examples/01_HelloWorld/01_HelloWorld.ino")
        self.assertEqual("hello_world", idf.builds[0]["name"])
        self.assertEqual("01_HelloWorld", arduino.builds[0]["name"])

    def test_policy_and_manual_firmware_workflows_do_not_build_examples(self) -> None:
        paths = (
            "config/markdown-audit.json",
            ".github/workflows/repository-policy.yml",
            ".github/workflows/firmware.yml",
        )
        self.assertFalse(self.route("esp-idf", *paths).builds)
        self.assertFalse(self.route("arduino", *paths).builds)

    def test_firmware_is_reported_but_not_added_to_example_ci(self) -> None:
        for path in (
            "firmware/brookesia/main/main.cpp",
            "firmware/brookesia/README.md",
            "firmware/brookesia/factory.bin",
            "firmware/brookesia/package.zip",
        ):
            result = self.route("esp-idf", path)
            self.assertTrue(result.firmware_touched)
            self.assertFalse(result.builds)

    def test_unknown_non_documentation_input_fails_safe_to_full_matrix(self) -> None:
        self.assertEqual(26, len(self.route("esp-idf", "tools/new-generator.py").builds))
        self.assertEqual(10, len(self.route("arduino", "tools/new-generator.py").builds))

    def test_name_status_parser_keeps_deletions_and_both_rename_paths(self) -> None:
        payload = (
            b"D\0examples/esp-idf/hello_world/main/old.c\0"
            b"R100\0examples/arduino/HelloWorld/old.ino\0"
            b"examples/arduino/HelloWorld/HelloWorld.ino\0"
        )
        with tempfile.TemporaryDirectory() as directory:
            changed_file = Path(directory) / "name-status.z"
            changed_file.write_bytes(payload)
            paths = selector.parse_name_status(changed_file)
        self.assertEqual(3, len(paths))
        self.assertIn("examples/esp-idf/hello_world/main/old.c", paths)
        self.assertIn("examples/arduino/HelloWorld/old.ino", paths)
        self.assertIn("examples/arduino/HelloWorld/HelloWorld.ino", paths)

    def test_name_status_parser_handles_copy_and_text_formats(self) -> None:
        nul_payload = (
            b"C075\0examples/arduino/HelloWorld/HelloWorld.ino\0"
            b"examples/arduino/HelloWorld/Copy.ino\0"
            b"A\0examples/esp-idf/hello_world/main/new.c\0"
        )
        text_payload = (
            "A\texamples/esp-idf/hello_world/main/new.c\n"
            "D\texamples/esp-idf/hello_world/main/old.c\n"
            "R100\texamples/arduino/HelloWorld/old.ino\t"
            "examples/arduino/HelloWorld/HelloWorld.ino\n"
            "C090\texamples/arduino/HelloWorld/HelloWorld.ino\t"
            "examples/arduino/HelloWorld/Copy.ino\n"
        ).encode()
        with tempfile.TemporaryDirectory() as directory:
            nul_file = Path(directory) / "nul.z"
            text_file = Path(directory) / "text.txt"
            nul_file.write_bytes(nul_payload)
            text_file.write_bytes(text_payload)
            nul_paths = selector.parse_name_status(nul_file)
            text_paths = selector.parse_name_status(text_file)
        self.assertEqual(3, len(nul_paths))
        self.assertEqual(5, len(text_paths))

    def test_name_status_parser_rejects_malformed_rename(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            changed_file = Path(directory) / "bad.z"
            changed_file.write_bytes(b"R100\0examples/arduino/HelloWorld/old.ino\0")
            with self.assertRaises(selector.RoutingError):
                selector.parse_name_status(changed_file)

    def test_path_normalization_handles_windows_and_rejects_escape(self) -> None:
        self.assertEqual(
            "examples/arduino/HelloWorld/HelloWorld.ino",
            selector.normalize_path(r".\examples\arduino\HelloWorld\HelloWorld.ino"),
        )
        for invalid in ("../outside", "/absolute/path", r"C:\Users\example\file"):
            with self.assertRaises(selector.RoutingError):
                selector.normalize_path(invalid)

    def test_github_output_is_single_line_json_and_boolean_text(self) -> None:
        selection = self.route("arduino", "examples/arduino/examples/01_HelloWorld/01_HelloWorld.ino")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "github-output.txt"
            selector.write_github_output(output, selection)
            lines = output.read_text(encoding="utf-8").splitlines()
        values = dict(line.split("=", 1) for line in lines)
        self.assertEqual("true", values["has_builds"])
        self.assertEqual("false", values["firmware_touched"])
        self.assertEqual(1, len(json.loads(values["builds"])))

    def test_empty_changed_file_scope_is_an_operational_error(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            changed_file = Path(directory) / "empty-name-status.z"
            changed_file.write_bytes(b"")
            with self.assertRaises(selector.RoutingError):
                selector.parse_name_status(changed_file)

    def test_exact_cli_manual_invocation(self) -> None:
        completed = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--root",
                str(ROOT),
                "--framework",
                "esp-idf",
                "--all",
                "--requested",
                "hello_world",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        payload = json.loads(completed.stdout)
        self.assertEqual(["examples/esp-idf/hello_world"], payload["selected"])
        self.assertEqual(2, len(payload["builds"]))


if __name__ == "__main__":
    unittest.main()
