"""ROM-free CSV preflight contract; does not constitute real gameplay parity."""
import csv
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'instrumentation'))
import verify_fcde_trace_gate_v11 as gate


class Preflight(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'trace.csv'
        self.rom = bytearray(393232)
        for pc in gate.SITES:
            offset = 0x38010 + pc - 0x8000
            self.rom[offset:offset + 3] = b'\x20\xde\xfc'
        self.hash = hashlib.sha256(self.rom).hexdigest()
        ram = bytearray(2048)
        ram[0x23] = 14
        ram[0x1fc] = 0x47
        ram[0x1fd] = 0x83
        self.row = dict.fromkeys(gate.FIELDS, '0')
        self.row.update(caller_pc=str(0x8345), entry_cycle='100',
                        return_cycle='180', entry_sp=str(0xfb),
                        return_sp=str(0xfd), entry_ram_hex=ram.hex(),
                        return_ram_hex=ram.hex())
        self.write()

    def write(self):
        with self.path.open('w', newline='', encoding='utf-8') as file:
            writer = csv.DictWriter(file, fieldnames=gate.FIELDS)
            writer.writeheader()
            writer.writerow(self.row)

    def check(self):
        with patch.object(gate, 'ROM_HASH', self.hash):
            return gate.parse_rows(self.path, self.rom)

    def test_valid_synthetic_schema(self):
        rows = self.check()
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['caller_pc'], '$8345')

    def test_stack_must_match_authenticated_jsr(self):
        self.row['entry_sp'] = '250'
        self.write()
        with self.assertRaisesRegex(ValueError, 'stack return mismatch'):
            self.check()

    def test_bad_site_and_bad_bank_refused(self):
        self.row['caller_pc'] = str(0x8158)
        self.write()
        with self.assertRaisesRegex(ValueError, 'not instrumented'):
            self.check()
        self.row['caller_pc'] = str(0x8345)
        ram = bytearray.fromhex(self.row['entry_ram_hex'])
        ram[0x23] = 7
        self.row['entry_ram_hex'] = ram.hex()
        self.write()
        with self.assertRaisesRegex(ValueError, 'bank-14'):
            self.check()

    def test_reject_missing_fields_cycle_and_corrupt_ram(self):
        self.row['return_cycle'] = '100'
        self.write()
        with self.assertRaisesRegex(ValueError, 'nonpositive'):
            self.check()
        self.row['return_cycle'] = '180'
        self.row['return_ram_hex'] = '00'
        self.write()
        with self.assertRaisesRegex(ValueError, '2048'):
            self.check()

    def test_wrong_rom_refused(self):
        with self.assertRaisesRegex(ValueError, 'size/SHA256'):
            gate.parse_rows(self.path, self.rom)


if __name__ == '__main__':
    unittest.main()
