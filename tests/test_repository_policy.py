from __future__ import annotations

import importlib.util
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "check_repository_policy.py"
SPEC = importlib.util.spec_from_file_location("check_repository_policy", SCRIPT)
assert SPEC and SPEC.loader
policy = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = policy
SPEC.loader.exec_module(policy)


class RepositoryPolicyTests(unittest.TestCase):
    def write_fixture(self, root: Path, relative: str, content: str) -> Path:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def test_maintained_bilingual_contract(self) -> None:
        self.assertEqual([], policy.check_bilingual_contract(ROOT))

    def test_single_product_homepage_contract(self) -> None:
        self.assertEqual([], policy.check_homepage_contract(ROOT))
        config = (ROOT / "config/markdown-audit.json").read_text(encoding="utf-8")
        self.assertIn('"category": "first_party_wrapper"', config)
        self.assertIn('Waveshare_ESP32_P4_4B_Display/README_ZH.md', config)
        expected_badges = ["Repository Policy", "ESP-IDF Build", "Arduino Build", "Firmware Build"]
        for relative, expected_alt in policy.README_HERO_ALTS.items():
            text = (ROOT / relative).read_text(encoding="utf-8")
            heroes = [image for image in policy._html_images(text) if image.get("src") == policy.README_HERO_PATH]
            self.assertEqual(1, len(heroes))
            self.assertEqual(expected_alt, heroes[0].get("alt"))
            self.assertEqual(expected_badges, [image.get("alt") for image in policy._html_images(text) if image.get("alt") in expected_badges])

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_fixture(root, "README.md", "<img src=\"docs/assets/esp32-p4-wifi6-touch-lcd-4b.jpg\" alt=\"wrong\">\n")
            self.write_fixture(root, "README_ZH.md", "<img src=\"docs/assets/esp32-p4-wifi6-touch-lcd-4b.jpg\" alt=\"wrong\">\n")
            self.write_fixture(root, "config/markdown-audit.json", "{}\n")
            errors = policy.check_homepage_contract(root)
            self.assertIn("Homepage hero is missing: docs/assets/esp32-p4-wifi6-touch-lcd-4b.jpg", errors)
            self.assertIn("README.md: homepage hero alt must be the localized product description", errors)
            self.assertIn("config/markdown-audit.json: homepage_pairs must declare the homepage contract", errors)

    def test_ci_boundaries(self) -> None:
        self.assertEqual([], policy.check_ci_contract(ROOT))

    def test_arduino_serial_startup_contract_ignores_comments_and_requires_short_bounds(self) -> None:
        self.assertEqual([], policy.check_arduino_serial_contract(ROOT))
        inventory = {path.relative_to(ROOT).as_posix() for path in policy.first_party_arduino_sources(ROOT)}
        self.assertIn("examples/arduino/HelloWorld/HelloWorld.ino", inventory)
        self.assertIn(
            "examples/arduino/libraries/Waveshare_ESP32_P4_4B_Display/displays_config.h",
            inventory,
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_fixture(
                root,
                ".github/workflows/arduino.yml",
                "ARDUINO_FQBN: esp32:esp32:esp32p4:USBMode=default,CDCOnBoot=default\n",
            )
            for relative in ("examples/arduino/README.md", "examples/arduino/README_ZH.md"):
                self.write_fixture(
                    root,
                    relative,
                    "Arduino-ESP32 3.3.11 USBMode=default CDCOnBoot=default UART0 CH343P\n",
                )
            sketch = self.write_fixture(
                root,
                "examples/arduino/Test/Test.ino",
                "// while (!Serial);\n"
                "const char *example = \"while (!USBSerial.dtr())\";\n"
                "void fatal() { while (true) {} }\n"
                "void setup() { unsigned long started = millis(); "
                "while (!Serial && (millis() - started) < 3000UL) {} }\n",
            )
            self.assertEqual([], policy.check_arduino_serial_contract(root))
            unsafe_conditions = (
                "while (!Serial) {}",
                "while (!USBSerial.dtr()) {}",
                "while (!SerialUSB) {}",
                "while (!Serial0) {}",
                "while (Serial.availableForWrite() < 1) {}",
                "while (!Serial && millis() - started < 60000UL) {}",
                "while (!Serial || (millis() - started) < 3000UL) {}",
                "while ((!USBSerial.dtr()) || ((millis() - started) <= 5000UL)) {}",
                "while (!(Serial)) {}",
                "while ((bool)Serial == false) {}",
                "while (Serial == false) {}",
                "for (; !Serial;) {}",
                "while (!Serial && ((millis() - started) < 3000UL ? true : true)) {}",
                "while (!Serial && ((millis() - started, 1) < 3000UL)) {}",
                "while (!Serial && ((started = millis()) < 3000UL)) {}",
                "while (!Serial && (((millis() - started) < 3000UL) & ready)) {}",
            )
            for condition in unsafe_conditions:
                sketch.write_text(f"void setup() {{ {condition} }}\n", encoding="utf-8")
                errors = policy.check_arduino_serial_contract(root)
                self.assertEqual(1, len(errors), (condition, errors))
                self.assertIn("unbounded Serial/USB CDC readiness wait", errors[0])

    def test_public_text_covers_every_sensitive_rule_family(self) -> None:
        sensitive = (
            "/home/example/work/repository",
            "Flash with COM17.",
            "Device MAC: 12:34:56:78:9A:BC",
            "Authorization: Bearer abcdefghijklmnopqrstuvwxyz1234",
            "Generated by Codex",
            "Open .codex/session.log",
        )
        safe = (
            "Use <REPOSITORY_PATH> and <PORT>.",
            "Device MAC: XX:XX:XX:XX:XX:XX",
            "Use a repository-relative path.",
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bad = self.write_fixture(root, "bad.md", "\n".join(sensitive))
            good = self.write_fixture(root, "good.md", "\n".join(safe))
            errors = policy.check_public_text(root, (bad, good))
        self.assertEqual(len(sensitive), len(errors), errors)
        self.assertTrue(all(error.startswith("bad.md:") for error in errors))

    def test_firmware_uses_registry_managed_waveshare_bsp(self) -> None:
        self.assertEqual([], policy.check_managed_component_contract(ROOT))

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_fixture(
                root,
                "firmware/brookesia/components/bsp_extra/idf_component.yml",
                "dependencies:\n"
                "  waveshare/esp32_p4_wifi6_touch_lcd_4b:\n"
                '    version: "3.0.1"\n',
            )
            self.write_fixture(
                root,
                f"{policy.FORBIDDEN_LOCAL_REUSABLE_COMPONENTS[0]}/idf_component.yml",
                "version: 3.0.0\n",
            )
            errors = policy.check_managed_component_contract(root)
            self.assertEqual(1, len(errors))
            self.assertIn("shadows ESP Component Registry", errors[0])

            manifest = root / "firmware/brookesia/components/bsp_extra/idf_component.yml"
            manifest.write_text(
                "dependencies:\n"
                "  waveshare/esp32_p4_wifi6_touch_lcd_4b:\n"
                '    version: "2.0.0"\n',
                encoding="utf-8",
            )
            errors = policy.check_managed_component_contract(root)
            self.assertTrue(any("must pin Registry" in error for error in errors))

    def test_revision_profile_policy_keeps_examples_single_profile_and_firmware_dual_profile(self) -> None:
        self.assertEqual([], policy.check_revision_profile_contract(ROOT))
        shared = (ROOT / "config/sdkconfig.defaults").read_text(encoding="utf-8")
        self.assertIn("CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n", shared)
        self.assertIn("CONFIG_ESP32P4_REV_MIN_300=y", shared)
        self.assertIn("CONFIG_SPIRAM_SPEED_250M=y", shared)
        for defaults in (ROOT / "examples/esp-idf").glob("**/sdkconfig.defaults*"):
            if "managed_components" in defaults.relative_to(ROOT).parts:
                continue
            text = defaults.read_text(encoding="utf-8")
            self.assertNotIn("CONFIG_SPIRAM_SPEED_200M=y", text, defaults)
            self.assertNotIn("CONFIG_SPIRAM_SPEED=200", text, defaults)
        firmware = (ROOT / ".github/workflows/firmware.yml").read_text(encoding="utf-8")
        self.assertIn("profile: rev1_3", firmware)
        self.assertIn("profile: rev3_x", firmware)
        self.assertIn("build-${{ matrix.profile }}", firmware)
        self.assertIn("$GITHUB_WORKSPACE/firmware/brookesia/build-${{ matrix.profile }}", firmware)
        self.assertIn("$RUNNER_TEMP/brookesia-${{ matrix.profile }}.sdkconfig", firmware)
        base_defaults = (ROOT / "firmware/brookesia/sdkconfig.defaults").read_text(encoding="utf-8")
        self.assertIn("CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y", base_defaults)
        self.assertIn("CONFIG_ESP32P4_REV_MIN_100=y", base_defaults)
        self.assertIn("CONFIG_SPIRAM_SPEED_200M=y", base_defaults)
        self.assertNotIn("CONFIG_SPIRAM_SPEED_250M=y", base_defaults)
        self.assertIn("CONFIG_PARTITION_TABLE_OFFSET=0x8000", base_defaults)
        rev3_defaults = (ROOT / "firmware/brookesia/sdkconfig.defaults.rev3_x").read_text(encoding="utf-8")
        self.assertIn("CONFIG_BOOTLOADER_LOG_LEVEL_ERROR=y", rev3_defaults)
        self.assertIn("CONFIG_BOOTLOADER_LOG_LEVEL=1", rev3_defaults)
        partitions = (ROOT / "firmware/brookesia/partitions.csv").read_text(encoding="utf-8")
        self.assertIn("factory,  app,  factory,        0x00200000,     8M,", partitions)
        for name, size in (("nvsfactory", "200K"), ("nvs", "840K"), ("otadata", "0x2000"), ("phy_init", "0x1000"), ("model", "0xF0000"), ("storage", "6M")):
            self.assertRegex(partitions, rf"(?m)^{name},[^\n]*,\s*,\s*{re.escape(size)},")

    def test_ci_artifact_contract_has_final_sha_and_non_erasing_flasher(self) -> None:
        expected = "actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a"
        for name in ("esp-idf.yml", "arduino.yml", "firmware.yml"):
            text = (ROOT / ".github/workflows" / name).read_text(encoding="utf-8")
            self.assertIn(expected, text)
            self.assertIn("retention-days: 14", text)
            self.assertIn("PACKAGE_GIT_SHA: ${{ github.event.pull_request.head.sha || github.sha }}", text)
        container_checkout_trust = 'git config --global --add safe.directory "$GITHUB_WORKSPACE"'
        for name in ("esp-idf.yml", "firmware.yml"):
            text = (ROOT / ".github/workflows" / name).read_text(encoding="utf-8")
            self.assertIn(container_checkout_trust, text)
        flasher = (ROOT / "scripts/ci_firmware.py").read_text(encoding="utf-8")
        self.assertIn("Hash of data verified", flasher)
        self.assertIn("c6_firmware_included", flasher)
        self.assertIn("silicon revision v3.00 or newer", flasher)
        self.assertIn("no PCB revision is inferred", flasher)
        self.assertNotIn("confirm matching rev3_x PCB/electrical revision", flasher)
        self.assertNotIn("erase_flash", flasher)
        self.assertIn("ci_firmware.py", (ROOT / "scripts/Flash-CI-Firmware.ps1").read_text(encoding="utf-8"))

    def test_idf_partition_contract(self) -> None:
        self.assertEqual([], policy.check_idf_partition_contract(ROOT))

    def test_idf_partition_contract_ignores_generated_managed_components(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_fixture(root, "config/sdkconfig.defaults", "CONFIG_PARTITION_TABLE_OFFSET=0x10000\n")
            self.write_fixture(
                root,
                "examples/esp-idf/demo/managed_components/vendor/partitions.csv",
                "factory, app, factory, 0x10000, 1M\n",
            )
            self.assertEqual([], policy.check_idf_partition_contract(root))

    def test_idf_component_names_cover_both_supported_lines(self) -> None:
        i2c_cmake = (ROOT / "examples/esp-idf/i2c-tools/main/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        lcd_cmake = (ROOT / "examples/esp-idf/lcd-color-test/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        usb_cmake = (
            ROOT / "examples/esp-idf/usb-extended-screen/main/CMakeLists.txt"
        ).read_text(encoding="utf-8")
        self.assertIn("REQUIRES console ", i2c_cmake)
        self.assertNotIn("tools/unit-test-app/components", lcd_cmake)
        self.assertIn("espressif__usb_device_uac", usb_cmake)
        self.assertIn(
            """PRIV_REQUIRES
                       esp_driver_jpeg
                       bsp_extra
                       usb""",
            usb_cmake,
        )
        usb_bsp_extra_manifest = (
            ROOT
            / "examples/esp-idf/usb-extended-screen/components/bsp_extra/idf_component.yml"
        ).read_text(encoding="utf-8")
        self.assertIn(
            'chmorgan/esp-audio-player:\n    version: "1.1.0"\n    public: true',
            usb_bsp_extra_manifest,
        )
        self.assertNotIn('chmorgan/esp-audio-player:\n    version: "1.0.7"', usb_bsp_extra_manifest)

    def test_first_party_pair_inventory_detects_orphan_and_excludes_upstream_trees(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_fixture(root, "docs/guide.md", "# Guide\n")
            self.write_fixture(root, "examples/arduino/libraries/upstream/README.md", "# Upstream\n")
            self.write_fixture(root, "firmware/brookesia/components/upstream/README.md", "# Upstream\n")
            inventory = {path.relative_to(root).as_posix() for path in policy.first_party_markdown(root)}
            self.assertIn("docs/guide.md", inventory)
            self.assertNotIn("examples/arduino/libraries/upstream/README.md", inventory)
            self.assertNotIn("firmware/brookesia/components/upstream/README.md", inventory)
            self.assertIn(
                "Missing maintained bilingual pair: docs/guide.md / docs/guide_ZH.md",
                policy.check_bilingual_contract(root),
            )

    def test_first_party_inventory_covers_declared_maintained_surfaces(self) -> None:
        inventory = {path.relative_to(ROOT).as_posix() for path in policy.first_party_markdown(ROOT)}
        expected = {
            "README.md",
            "SUPPORT.md",
            "CONTRIBUTING.md",
            "THIRD_PARTY_NOTICES.md",
            ".github/ISSUE_TEMPLATE/bug_report.md",
            ".github/pull_request_template.md",
            "config/README.md",
            "docs/ci.md",
            "hardware/README.md",
            "releases/README.md",
            "schematic/README.md",
            "examples/arduino/AsciiTable/README.md",
            "examples/arduino/libraries/Waveshare_ESP32_P4_4B_Display/README.md",
            "examples/arduino/libraries/Waveshare_ESP32_P4_4B_Display/README_ZH.md",
            "examples/esp-idf/hello_world/README.md",
            "firmware/brookesia/README.md",
            "firmware/brookesia/archive/README.md",
        }
        self.assertTrue(expected <= inventory)
        self.assertNotIn(
            "firmware/brookesia/archive/components/AIChats/third_party/xiaozhi_esp32/README.md",
            inventory,
        )

    def test_arduino_wrapper_pair_detects_missing_page_and_language_switches(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            wrapper = "examples/arduino/libraries/Waveshare_ESP32_P4_4B_Display"
            self.write_fixture(root, f"{wrapper}/README.md", "# Wrapper\n")
            errors = policy.check_bilingual_contract(root)
            self.assertIn(
                f"Missing maintained bilingual pair: {wrapper}/README.md / {wrapper}/README_ZH.md",
                errors,
            )
            self.write_fixture(root, f"{wrapper}/README_ZH.md", "# 辅助库\n")
            errors = policy.check_bilingual_contract(root)
            self.assertIn(f"{wrapper}/README.md: missing language switch to {wrapper}/README_ZH.md", errors)
            self.assertIn(f"{wrapper}/README_ZH.md: missing language switch to {wrapper}/README.md", errors)

    def test_reciprocal_language_navigation_is_required_in_both_directions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_fixture(root, "docs/guide.md", "# Guide\n")
            self.write_fixture(root, "docs/guide_ZH.md", "# 指南\n")
            errors = policy.check_bilingual_contract(root)
            self.assertIn("docs/guide.md: missing language switch to docs/guide_ZH.md", errors)
            self.assertIn("docs/guide_ZH.md: missing language switch to docs/guide.md", errors)

    def test_local_fragments_support_page_local_url_decoding_and_duplicate_headings(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            guide_text = "# 标题\n## Repeat\n## Repeat\n## foo\n## foo-1\n## foo\n## keep--dash\n"
            guide = self.write_fixture(root, "docs/guide.md", guide_text)
            self.assertEqual(
                {"标题", "repeat", "repeat-1", "foo", "foo-1", "foo-2", "keep--dash"},
                policy.github_heading_slugs(guide_text),
            )
            source = self.write_fixture(
                root,
                "docs/source.md",
                "# 本页\n[中文](guide.md#%E6%A0%87%E9%A2%98) [repeat](guide.md#repeat-1) [collision](guide.md#foo-2) [hyphen](guide.md#keep--dash) [self](#本页)\n",
            )
            self.assertEqual([], policy.check_links(root, (guide, source)))
            source.write_text("# 本页\n[missing](guide.md#absent) [self](#不存在)\n", encoding="utf-8")
            errors = policy.check_links(root, (guide, source))
            self.assertEqual(2, len(errors))
            self.assertTrue(all("missing Markdown fragment" in error for error in errors))

    def test_same_language_local_markdown_routes_to_companion(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_fixture(root, "docs/guide.md", "[中文](guide_ZH.md)\n# Guide\n")
            self.write_fixture(root, "docs/guide_ZH.md", "[English](guide.md)\n# 指南\n")
            self.write_fixture(root, "docs/source.md", "[中文](source_ZH.md)\n# Source\n")
            self.write_fixture(root, "docs/source_ZH.md", "[English](source.md) [错误](guide.md)\n# 来源\n")
            errors = policy.check_bilingual_contract(root)
            self.assertEqual(1, len(errors))
            self.assertIn("wrong-language local link guide.md; expected docs/guide_ZH.md", errors[0])

    def test_homepage_firmware_quick_links_and_review_dates(self) -> None:
        english = (ROOT / "README.md").read_text(encoding="utf-8")
        chinese = (ROOT / "README_ZH.md").read_text(encoding="utf-8")
        self.assertIn('<a href="docs/firmware.md">📦 Firmware</a>', english)
        self.assertIn('<a href="docs/firmware_ZH.md">📦 固件</a>', chinese)
        self.assertEqual(policy.README_H2_ICONS, tuple(policy.H2_RE.findall(english)))
        self.assertEqual(policy.README_H2_ICONS, tuple(policy.H2_RE.findall(chinese)))
        for relative in ("README.md", "README_ZH.md", "docs/ci.md", "docs/ci_ZH.md"):
            self.assertIn("2026-08-10", (ROOT / relative).read_text(encoding="utf-8"))

    def test_arduino_autogenerated_prototypes_have_matching_attributes(self) -> None:
        for relative in (
            "examples/arduino/GFX_ESPWiFiAnalyzer/GFX_ESPWiFiAnalyzer.ino",
            "examples/arduino/LVGLV9_Arduino/LVGLV9_Arduino.ino",
        ):
            self.assertNotIn("[[noreturn]]", (ROOT / relative).read_text(encoding="utf-8"))

    def test_lvgl_and_usb_compatibility_contract(self) -> None:
        lvgl_projects = ("display-panel", "lvgl-v8", "lvgl-v9", "usb-extended-screen")
        for project in lvgl_projects:
            manifest = (
                ROOT / f"examples/esp-idf/{project}/main/idf_component.yml"
            ).read_text(encoding="utf-8")
            self.assertIn('espressif/esp_lvgl_adapter:\n    version: "0.6.3"', manifest)

        lvgl9_projects = ("display-panel", "lvgl-v9", "usb-extended-screen")
        for project in lvgl9_projects:
            manifest = (
                ROOT / f"examples/esp-idf/{project}/main/idf_component.yml"
            ).read_text(encoding="utf-8")
            defaults = (ROOT / f"examples/esp-idf/{project}/sdkconfig.defaults").read_text(
                encoding="utf-8"
            )
            self.assertIn('lvgl/lvgl:\n    version: "9.3.0"', manifest)
            self.assertIn("CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=n", defaults)
            self.assertNotIn("CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y", defaults)

        lvgl_v8_cmake = (ROOT / "examples/esp-idf/lvgl-v8/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn("lv_disp_rotation_t=lv_disp_rot_t", lvgl_v8_cmake)

        usb_defaults = (
            ROOT / "examples/esp-idf/usb-extended-screen/sdkconfig.defaults"
        ).read_text(encoding="utf-8")
        self.assertIn("CONFIG_USB_DEVICE_UAC_AS_PART=y", usb_defaults)

    def test_exact_policy_cli(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), "--root", str(ROOT)],
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        self.assertEqual(0, completed.returncode, completed.stderr)
        self.assertIn("Repository policy:", completed.stdout)


if __name__ == "__main__":
    unittest.main()
