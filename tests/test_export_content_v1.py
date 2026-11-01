"""ROM-only extraction contract: rejects unknown ROMs; no copyrighted fixtures."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/extract/export_content_v1.py"
spec = importlib.util.spec_from_file_location("rom_content_v1", SCRIPT)
assert spec and spec.loader
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class ROMContentTests(unittest.TestCase):
    def test_rom_sha256_is_locked(self):
        self.assertEqual(
            tool.ROM_SHA,
            "958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28"
        )
        self.assertEqual(tool.ROM_BYTES, 393232)
        self.assertEqual((tool.STAGES, tool.ROOMS), (15, 135))

    def test_wrong_rom_is_rejected_before_output(self):
        with tempfile.TemporaryDirectory() as td:
            output = Path(td) / "private-generated"
            with self.assertRaisesRegex(ValueError, "authenticated Japanese"):
                tool.extract(b"NES\x1a" + bytes(16), Path(td), output)
            self.assertFalse(output.exists())

    def test_fake_right_size_rom_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            output = Path(td) / "private-generated"
            fake = b"NES\x1a" + bytes(tool.ROM_BYTES - 4)
            with self.assertRaises(ValueError):
                tool.extract(fake, Path(td), output)
            self.assertFalse(output.exists())

    def test_room_contract_requires_unresolved_placement_to_be_null(self):
        schema = json.loads(
            (ROOT / "data/schemas/rom-room-v1.schema.json").read_text("utf-8")
        )
        self.assertIn("world_origin_status", schema["required"])
        self.assertIn("world_origin", schema["required"])
        self.assertEqual(schema["allOf"][0]["then"]["properties"]["world_origin"],
                         {"type": "null"})

    def test_no_fixed_generated_content_is_tracked(self):
        self.assertFalse((ROOT / "data/generated/jp-rom-content-v1").exists())


if __name__ == "__main__":
    unittest.main()
