"""Safety checks for the one-process ROM/PPU capture launcher."""
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/windows/Capture-CV3DCJP-SynchronizedV3.ps1"
TEXT = SCRIPT.read_text("utf-8")


class Stage0CaptureContract(unittest.TestCase):
    def test_capture_and_writer_share_one_process(self):
        self.assertIn("CVIII_SELECTOR_TRACE_5H8U", TEXT)
        self.assertIn("--cviii-diag-prefix", TEXT)
        self.assertIn("--cviii-diag-mode", TEXT)
        self.assertIn("& $exe @gameArgs", TEXT)
        self.assertIn("capture_frame", (ROOT / "tools/extract/verify_stage0_synchronized_v3.py").read_text())

    def test_protected_game_is_copied_not_rebuilt(self):
        self.assertIn("Copy-Item -LiteralPath", TEXT)
        self.assertIn("data\\generated\\stage0-sync-v3", TEXT)
        self.assertNotIn("cmake --build", TEXT)
        self.assertNotIn("Build-CVIII-Phase5H8F.ps1", TEXT)
        self.assertIn("--edition','original'", TEXT)

    def test_powershell_parses_when_available(self):
        shell = shutil.which("pwsh")
        if not shell:
            self.skipTest("PowerShell 7 not installed on this test host")
        # A parser check does not execute the script or launch any ROM.
        cmd = ("$t=$null;$e=$null;"
               "[System.Management.Automation.Language.Parser]::ParseFile("
               "$args[0],[ref]$t,[ref]$e)|Out-Null;"
               "if($e.Count){foreach($x in $e){[Console]::Error.WriteLine($x.Message + ' at line ' + $x.Extent.StartLineNumber)};exit 1}")
        result = subprocess.run([shell, "-NoProfile", "-Command", cmd,
                                 str(SCRIPT)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
