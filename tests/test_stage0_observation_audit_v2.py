"""Fail-closed tests for Stage 0 capture/ROM evidence auditing."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools/extract/audit_stage0_observations_v2.py"
spec = importlib.util.spec_from_file_location("stage0_audit_v2", SOURCE)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class Stage0AuditTests(unittest.TestCase):
    def test_exact_rom_sha_and_frame_hash_anchors(self):
        self.assertEqual(len(tool.ROM_SHA), 64)
        self.assertEqual(len(tool.FRAMES_SHA), 64)
        self.assertEqual(len(tool.EVENTS_SHA), 64)
        self.assertEqual(tool.SCHEMA, "cv3dcjp.stage0-evidence/v2")

    def test_original_capture_fnv_is_not_authentication_hash(self):
        self.assertEqual(tool.fnv32(b""), 2166136261)
        self.assertEqual(tool.fnv32(b"hello"), 0x4f9f2cab)
        self.assertEqual(
            tool.sha(b"hello"),
            "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
        )

    def test_wrong_rom_does_not_create_output(self):
        with tempfile.TemporaryDirectory() as td:
            destination = Path(td) / "out"
            with self.assertRaisesRegex(ValueError, "Authenticated Japanese ROM"):
                tool.audit(b"NES\x1a", Path(td)/"missing.zip",
                           Path(td)/"missing-content.zip", destination)
            self.assertFalse(destination.exists())

    def test_destination_is_protected_even_if_capture_missing(self):
        with tempfile.TemporaryDirectory() as td:
            destination = Path(td) / "out"
            destination.mkdir()
            with self.assertRaises(ValueError):
                tool.audit(b"", Path(td)/"missing.zip",
                           Path(td)/"missing-content.zip", destination)
            self.assertTrue(destination.exists())

    def test_no_generated_private_assets_in_public_repository(self):
        self.assertFalse((ROOT / "data/generated/stage0-evidence-v2").exists())


if __name__ == "__main__":
    unittest.main()
