from __future__ import annotations

import importlib.util
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

    def test_ci_artifact_contract_has_final_sha_and_non_erasing_flasher(self) -> None:
        expected = "actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a"
        for name in ("esp-idf.yml", "arduino.yml", "firmware.yml"):
            text = (ROOT / ".github/workflows" / name).read_text(encoding="utf-8")
            self.assertIn(expected, text)
            self.assertIn("retention-days: 14", text)
            self.assertIn("PACKAGE_GIT_SHA: ${{ github.event.pull_request.head.sha || github.sha }}", text)
        flasher = (ROOT / "scripts/Flash-CI-Firmware.ps1").read_text(encoding="utf-8")
        self.assertIn("Hash of data verified", flasher)
        self.assertIn("c6_firmware_included", flasher)
        self.assertNotIn("erase_flash", flasher)

    def test_idf_partition_contract(self) -> None:
        self.assertEqual([], policy.check_idf_partition_contract(ROOT))

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
            "examples/esp-idf/hello_world/README.md",
            "firmware/brookesia/README.md",
            "firmware/brookesia/archive/README.md",
        }
        self.assertTrue(expected <= inventory)
        self.assertNotIn(
            "firmware/brookesia/archive/components/AIChats/third_party/xiaozhi_esp32/README.md",
            inventory,
        )

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
