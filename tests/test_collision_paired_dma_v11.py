"""ROM-free paired trace contract tests: mocked ROM reference, never parity proof."""
import csv
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'instrumentation'))
import verify_fcde_paired_dma_v11 as paired
import verify_fcde_trace_gate_v11 as gate


class PairedContract(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.rom=self.root/'fake.nes'
        self.rom.write_bytes(b'test: no ROM')
        self.mesen=self.root/'mesen.csv'
        self.native=self.root/'native.csv'
        self.dma=self.root/'dma.csv'
        self.report=self.root/'report.json'
        ram=bytearray(2048)
        ram[0x23]=14
        ram[0x1fc]=0x2b
        ram[0x1fd]=0x84
        self.row={col:'0' for col in gate.FIELDS}
        self.row.update(caller_pc=str(0x8429),entry_cycle='10000',
                        return_cycle='10156',entry_sp='251',
                        return_sp='253',entry_ram_hex=ram.hex(),
                        return_ram_hex=ram.hex())
        self.native_row=self.row.copy()
        self.native_row['entry_cycle']='50000'
        self.native_row['return_cycle']='50156'
        self.dma_row=dict(sample='1',caller_pc=str(0x8429),
                          entry_cycle='50000',return_cycle='50156',
                          instruction_cycles='152',dma_cycles='4',
                          dma_halt_cycles='3',dma_read_cycles='1',
                          dma_write_cycles='0',dmc_fetches_confirmed='1',
                          oam_dma_writes='0')
        self.write()

    def write(self):
        for path,fields,row in ((self.mesen,gate.FIELDS,self.row),
                                (self.native,gate.FIELDS,self.native_row),
                                (self.dma,paired.DMA_COLUMNS,self.dma_row)):
            with path.open('w',newline='',encoding='utf-8') as f:
                w=csv.DictWriter(f,fieldnames=fields)
                w.writeheader();w.writerow(row)

    def verify(self):
        with patch.object(paired.gate,'parse_rows'), \
             patch.object(paired,'reference_cycles',return_value=152):
            return paired.verify(self.rom,self.mesen,self.native,
                                 self.dma,self.report)

    def test_synthetic_pair_and_dma_stall_accounting(self):
        r=self.verify()
        self.assertEqual((r['samples'],r['pairs_matching']),(1,1))
        self.assertFalse(r['dmc_dma_causality_verified'])
        self.assertFalse(r['native_substitution_authorized'])

    def test_unsynced_entry_fails_closed(self):
        self.native_row['entry_a']='5'
        self.write()
        r=self.verify()
        self.assertIn('entry_state_not_synchronized',
                      r['failures'][0]['differences'])

    def test_unexplained_native_clock_fails(self):
        self.dma_row['dma_cycles']='0'
        self.write()
        r=self.verify()
        self.assertIn('native_dma_time_not_accounted',
                      r['failures'][0]['differences'])

    def test_unmatched_total_cycles_fails(self):
        self.native_row['return_cycle']='50152'
        self.dma_row.update(return_cycle='50152',dma_cycles='0',
                            dma_halt_cycles='0',dma_read_cycles='0',
                            dmc_fetches_confirmed='0')
        self.write()
        r=self.verify()
        self.assertIn('cross_emulator_total_cycles',
                      r['failures'][0]['differences'])

    def test_extra_dma_sample_rejected(self):
        self.dma_row['sample']='2'
        self.write()
        r=self.verify()
        self.assertIn('dma_sequence',r['failures'][0]['differences'])

    def test_incorrect_rom_preflight_must_reject(self):
        with self.assertRaisesRegex(ValueError,'size/SHA256'):
            paired.verify(self.rom,self.mesen,self.native,
                          self.dma,self.report)


if __name__=='__main__':
    unittest.main()
