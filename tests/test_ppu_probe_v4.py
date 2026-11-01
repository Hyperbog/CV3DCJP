"""ROM-free source identity, safety and Windows PowerShell syntax checks."""
import importlib.util
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/instrumentation/prepare_ppu_probe_v4.py"
spec = importlib.util.spec_from_file_location("probe_v4", SCRIPT)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ProbeV4Tests(unittest.TestCase):
    def test_pinned_source_identities(self):
        self.assertEqual(len(probe.PINNED), 3)
        self.assertTrue(all(len(v) == 64 for v in probe.PINNED.values()))

    def test_disabled_without_environment_opt_in(self):
        self.assertIn('getenv("CVIII_PPU_PROBE_V4")', probe.ENGINE_IMPL)
        self.assertIn('if (!cviii_v4_enabled || !cviii_v4_file)', probe.ENGINE_IMPL)
        self.assertIn('"wx"', probe.ENGINE_IMPL)

    def test_distinct_mutation_events_and_no_false_producer_binding(self):
        docs = (ROOT / "docs/STAGE0_PPU_V4.md").read_text("utf-8")
        for name in ("PALETTE_COMMIT", "PALETTE_CORRUPTION",
                     "VRC6_CHR_MAP", "CPU_PPU_REQUEST", "CPU_VRC6_REQUEST"):
            self.assertIn(name, docs)
        self.assertIn("LAST_FETCH_CONTEXT_ONLY", probe.ENGINE_IMPL)

    def test_source_anchor_drift_rejected(self):
        for name in probe.PINNED:
            with self.subTest(name=name), self.assertRaises(ValueError):
                probe.patch(name, "/* changed engine */\n")

    def test_changed_source_never_creates_destination(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            base = root / "engine/runner/cyc"
            base.mkdir(parents=True)
            (base / "project.cmake").write_text("CMake\n")
            for name in probe.PINNED:
                (base / name).write_text("source drift")
            dest = root / "isolated"
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                probe.prepare(root / "engine", dest)
            self.assertFalse(dest.exists())

    def test_source_and_destination_isolation(self):
        with tempfile.TemporaryDirectory() as td:
            source = Path(td) / "engine"
            source.mkdir()
            with self.assertRaises(FileExistsError):
                probe.prepare(source, source)
            with self.assertRaises(FileExistsError):
                probe.prepare(source, source / "inside")

    def test_no_working_baseline_overwrite(self):
        ps = (ROOT / "tools/instrumentation/Capture-Stage0-PPUV4.ps1").read_text()
        self.assertIn("Copy-Item -LiteralPath", ps)
        self.assertIn("data\\generated\\stage0-ppu-v4", ps)
        self.assertIn("--edition original --room-view 0", ps)
        self.assertNotIn("Build-CVIII-Phase5H8F.ps1", ps)

    def test_powershell_parses_on_windows(self):
        shell = shutil.which("pwsh")
        if shell is None:
            self.skipTest("pwsh not installed on CI host")
        script = ROOT / "tools/instrumentation/Capture-Stage0-PPUV4.ps1"
        cmd = ("$t=$null;$e=$null;"
               "[System.Management.Automation.Language.Parser]::ParseFile("
               "$env:CV3_PPU_PROBE_PARSE,[ref]$t,[ref]$e)|Out-Null;"
               "if($e.Count){foreach($x in $e){"
               "[Console]::Error.WriteLine($x.Message + ' line ' + "
               "$x.Extent.StartLineNumber)};exit 1}")
        env = dict(os.environ, CV3_PPU_PROBE_PARSE=str(script))
        result = subprocess.run([shell, "-NoProfile", "-Command", cmd],
                                capture_output=True, text=True, env=env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
