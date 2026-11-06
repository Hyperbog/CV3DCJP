"""ROM-free structural contracts for arbitrary-length owner native capture verifier.

These are not hardware or cross-emulator parity evidence.
"""
from pathlib import Path
import csv
import io
import sys
import unittest
from unittest.mock import patch
from zipfile import ZipFile
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'instrumentation'))
import verify_fcde_native_zip_v11 as native

class NativeV11ZipContracts(unittest.TestCase):
    def test_unsigned_rejects_invalid_field_values(self):
        for value in ('', '-1', '1.0', '0x10', ' 1', '+1'):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    native.unsigned({'value':value},'value',1)
        self.assertEqual(native.unsigned({'value':'212'},'value',1),212)

    def test_memory_entry_fingerprint_ignores_only_constant_status_bit(self):
        a=dict(caller_pc='33833',entry_a='5',entry_x='16',
               entry_y='4',entry_sp='239',entry_p='0',
               entry_ram_hex='a1'*2048)
        b=a.copy();b['entry_p']='32'
        self.assertEqual(native.key_for(a),native.key_for(b))
        b['entry_p']='33'
        self.assertNotEqual(native.key_for(a),native.key_for(b))
        b=a.copy();b['entry_ram_hex']='a2'*2048
        self.assertNotEqual(native.key_for(a),native.key_for(b))

    def test_arbitrary_length_native_rows_are_not_mesen_128_cap(self):
        data='x,y\n'+''.join(f'{i},{i+1}\n' for i in range(212))
        rows=native.parse_csv(data.encode(),('x','y'),'synthetic')
        self.assertEqual(len(rows),212)
        self.assertRaises(ValueError,native.parse_csv,
                          b'x,y\n1\n',('x','y'),'synthetic')

    def test_missing_dma_or_duplicate_capture_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'native.zip'
            with ZipFile(path,'w') as z:
                z.writestr('capture/native-fcde.csv',b'example')
            with self.assertRaisesRegex(ValueError,'missing owner native'):
                native.read_zip(path)
            with ZipFile(path,'w') as z:
                z.writestr('A/native-fcde.csv',b'x')
                z.writestr('B/native-fcde.csv',b'y')
            with self.assertRaisesRegex(ValueError,'duplicate'):
                native.read_zip(path)

if __name__=='__main__':
    unittest.main()
