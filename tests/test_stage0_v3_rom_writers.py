"""Synthetic tests for v3 instruction attribution; no user ROM is distributed."""
import importlib.util
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "tools/extract/verify_stage0_v3_rom_writers.py"
spec = importlib.util.spec_from_file_location("rom_writers_v3", SOURCE)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def event(pc=0xf97e, offset=0x3f97e, target=53, old=0, new=1):
    return {
        "pc": str(pc), "prg_offset": str(offset),
        "address": str(target), "old_value": str(old), "new_value": str(new),
        "fetch_a": "12", "fetch_x": "1", "fetch_y": "0",
        "zp00": "0", "zp01": "0"
    }


class WriterOpcodeTests(unittest.TestCase):
    def test_prg_offset_is_header_excluded(self):
        rom = bytearray(393232)
        row = event()
        rom[0x3f97e:0x3f980] = b"\x00\x00"  # Not the instruction bytes!
        rom[0x3f98e:0x3f990] = b"\xe6\x35"
        offset, opcode = audit.decode_writer(bytes(rom), row)
        self.assertEqual(offset, 0x3f98e)
        self.assertEqual(opcode, "INC zp")

    def test_rom_mapping_does_not_accept_wrong_file_offset(self):
        rom = bytearray(393232)
        rom[0x3f98e:0x3f990] = b"\xe6\x35"
        with self.assertRaisesRegex(ValueError, "mapping"):
            audit.decode_writer(bytes(rom), event(offset=0x3f97d))

    def test_indirect_y_effective_address(self):
        rom = bytearray(393232)
        rom[0x3e03a:0x3e03c] = b"\x91\x00"
        row = event(pc=0xe02a, offset=0x3e02a, target=54, old=240, new=0)
        row.update(fetch_a="0", fetch_y="54")
        self.assertEqual(audit.decode_writer(bytes(rom), row),
                         (0x3e03a, "STA ($00),Y"))

    def test_wrong_register_and_data_are_rejected(self):
        rom = bytearray(393232)
        rom[0x3e03a:0x3e03c] = b"\x91\x00"
        row = event(pc=0xe02a, offset=0x3e02a, target=54, old=240, new=7)
        row.update(fetch_a="0", fetch_y="54")
        with self.assertRaisesRegex(ValueError, "operand/CPU register"):
            audit.decode_writer(bytes(rom), row)

    def test_no_inferred_geometry_or_palette_source(self):
        contents = SOURCE.read_text("utf-8")
        self.assertIn('"new_verified_world_origins": 0', contents)
        self.assertIn('"palette_source_rom_addresses_verified": 0', contents)
        self.assertIn('"executable_sha_verified_from_uploaded_zip": False', contents)

    def test_wrong_rom_identity_rejected_before_zip_read(self):
        with self.assertRaisesRegex(ValueError, "Authenticated Japanese"):
            audit.verify(b"NES\x1a", Path("does-not-exist.zip"))


if __name__ == "__main__":
    unittest.main()
