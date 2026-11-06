"""ROM-free contracts for v12 true-source-identified DMC DMA bus gate.

Synthetic data NEVER constitutes parity evidence from the retail ROM.
"""
import io
import sys
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'instrumentation'))
import verify_fcde_dmc_bus_v12 as gate

class DmcBusV12(unittest.TestCase):
    def m(self,cycle=200,addr=0xd125,kind='R',confirmed=1):
        return dict(cpu_cycle=str(cycle),bus_kind=kind,address=str(addr),
                    value='75',cpu_pc=str(0xfce7),
                    real_dmc_sample_read=str(confirmed))

    def n(self,cycle=200,addr=0xd125,kind='R',confirmed=1):
        return dict(sample='1',caller_pc=str(0x96ed),cycle_index=str(cycle),
                    kind=kind,address=str(addr),value='75',data_bus='75',
                    halt='0',dmc_address_before=str(addr),
                    dmc_address_after=str(addr+1),dmc_remaining_before='5',
                    dmc_remaining_after='4',dmc_fetch_verified=str(confirmed),
                    instruction_done='0')

    def test_genuine_source_identifiers_required(self):
        self.assertEqual(gate.real_mesen_dmc_rows([self.m()]),[(200,0xd125)])
        self.assertEqual(gate.real_native_dmc_rows([self.n()]),
                         [(1,200,0xd125)])
        self.assertEqual(gate.real_mesen_dmc_rows([self.m(confirmed=0)]),[])
        self.assertEqual(gate.real_native_dmc_rows([self.n(confirmed=0)]),[])

    def test_dmc_cycle_not_silently_shifted(self):
        a=[(200,0xd125)]
        self.assertTrue(gate.compare_events(a,a)['same_exact_read_cycle_and_address'])
        self.assertFalse(gate.compare_events(a,[(204,0xd125)])
                         ['same_exact_read_cycle_and_address'])
        self.assertFalse(gate.compare_events(a,[(200,0xd126)])
                         ['same_exact_read_cycle_and_address'])

    def test_outside_call_does_not_count(self):
        self.assertEqual(gate.in_window([(201,12),(205,13),(210,14)],201,210),
                         [(201,12),(205,13)])

    def test_native_invalid_device_flag_fails_closed(self):
        with self.assertRaisesRegex(ValueError,'actual DMA read'):
            gate.real_native_dmc_rows([self.n(kind='W')])

    def test_mesen_false_dmc_write_fails_closed(self):
        with self.assertRaisesRegex(ValueError,'not a read'):
            gate.real_mesen_dmc_rows([self.m(kind='W')])

    def test_order_and_integer_validation(self):
        with self.assertRaisesRegex(ValueError,'not ordered'):
            gate.real_mesen_dmc_rows([self.m(204),self.m(200)])
        with self.assertRaises(ValueError):
            gate.number({'v':'0x100'},'v',255)

    def test_source_specific_fields_are_exact(self):
        self.assertEqual(len(gate.M_FIELDS),6)
        self.assertEqual(len(gate.N_FIELDS),14)
        self.assertEqual(len(gate.D_FIELDS),11)

if __name__=='__main__':
    unittest.main()
