"""Static safety contract for the isolated Windows ROM baseline launcher.

These checks do not substitute for actual MSVC, CTest, or live gameplay.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "tools/windows/Build-CV3DCJP-IsolatedBaseline.ps1").read_text(
    encoding="utf-8"
)


class IsolatedBaselineTests(unittest.TestCase):
    def test_rom_identity_is_pinned(self):
        self.assertIn(
            "958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28",
            SCRIPT,
        )
        self.assertIn("Get-FileHash", SCRIPT)

    def test_old_installation_is_protected(self):
        self.assertIn("C:\\NESDev\\AkumajouDensetsu", SCRIPT)
        self.assertIn("C:\\NESDev\\CVIII-Decomp", SCRIPT)
        self.assertIn("if ($build.Equals($protected", SCRIPT)
        self.assertIn("Refusing to overwrite source or working game", SCRIPT)

    def test_build_is_isolated_and_owner_marked(self):
        self.assertIn("C:\\NESDev\\CV3DCJP-Builds\\5h8f-original", SCRIPT)
        self.assertIn(".cv3dcjp-isolated-5h8f", SCRIPT)
        self.assertIn("Existing unmarked build directory", SCRIPT)

    def test_ui_is_disabled_and_legacy_launcher_not_invoked(self):
        self.assertIn("-DJP_MODERN_ENABLE_UI=OFF", SCRIPT)
        self.assertNotIn("-DJP_MODERN_ENABLE_UI=ON", SCRIPT)
        self.assertNotIn("launcher_theme.h", SCRIPT)
        self.assertNotIn("Build-CVIII-Phase5H8F.ps1", SCRIPT)

    def test_actual_build_and_gameplay_gates_are_separate(self):
        self.assertIn("ctest --test-dir", SCRIPT)
        self.assertIn("--edition original --room-view 0", SCRIPT)
        self.assertIn("gameplay remains unverified", SCRIPT)


if __name__ == "__main__":
    unittest.main()
