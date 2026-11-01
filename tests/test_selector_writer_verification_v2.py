"""Static original-ROM writer audit contract: never infer execution or placement."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/extract/verify_selector_writers_v2.py"
spec = importlib.util.spec_from_file_location("selector_writers_v2", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class SelectorWriterTests(unittest.TestCase):
    def test_opcodes_are_known_6502_zero_page_operations(self):
        self.assertEqual(module.OPCODES, {"INC": 0xE6, "DEC": 0xC6, "STA": 0x85})
        self.assertTrue(module.PATTERN.fullmatch("INC $36"))
        self.assertTrue(module.PATTERN.fullmatch("DEC $35"))
        self.assertTrue(module.PATTERN.fullmatch("STA $34"))
        self.assertIsNone(module.PATTERN.fullmatch("JMP $34"))

    def test_wrong_rom_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "authenticated Japanese"):
            module.verify(b"NES\x1a" + bytes(64), "")

    def test_no_guessed_geometry_is_reported(self):
        self.assertIn("new_world_placements", SCRIPT.read_text(encoding="utf-8"))
        self.assertIn("ROM_BYTES_ONLY_NOT_RUNTIME_TRANSITION_PROOF",
                      SCRIPT.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
