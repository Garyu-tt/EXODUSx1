"""Verify the legacy Wizard's packaged QuickTune CPU assets."""

from pathlib import Path
import re
import unittest


WIZARD = Path(__file__).resolve().parents[1] / "Old files" / "4ifir Wizard source code"
CPU_CONFIG = WIZARD / "SmartMode" / "QuickTune" / "CPU" / "config.ini"
PACKAGE_JSON = re.compile(
    r"(?m)^json_(?:mark_cur_kip|source)\s+'/switch/\.packages/4IFIR Wizard/([^']+\.json)'"
)


class WizardQuickTuneTests(unittest.TestCase):
    def test_cpu_menu_json_assets_are_in_the_wizard_payload(self):
        references = PACKAGE_JSON.findall(CPU_CONFIG.read_text(encoding="utf-8"))
        self.assertTrue(references, "Expected packaged QuickTune CPU assets")
        for reference in references:
            with self.subTest(reference=reference):
                self.assertTrue(
                    (WIZARD / reference).is_file(), f"Missing Wizard asset: {reference}"
                )


if __name__ == "__main__":
    unittest.main()
