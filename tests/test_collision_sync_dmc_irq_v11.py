"""ROM-free synthetic contracts only. Do not count as hardware parity evidence."""
import sys
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'instrumentation'))
import verify_fcde_sync_dmc_irq_v11 as v


class IrqSignatureContracts(unittest.TestCase):
    def make_irq_case(self):
        rom=bytearray(393232)
        rom[v.BASE+0x3ffe:v.BASE+0x4000]=(0xe0f5).to_bytes(2,'little')
        rom[v.BASE+0xe0f5-0xc000:v.BASE+0xe0f5-0xc000+11]=v.IRQ_PROLOGUE
        before=bytearray(2048)
        before[0]=0x75
        before[1]=0x93
        before[0x438]=0x24
        after=before.copy()
        sp=0xf1
        # ROM-matched expected pushes for the observed $FCEB IRQ site.
        saved=(0xfc,0xeb,0xa0,0xf0,0xf0,0x2c,0x75,0x93)
        changed=[]
        for i,val in enumerate(saved):
            addr=0x100+sp-i
            after[addr]=val
            changed.append(addr)
        row={'caller_pc':str(0x96ed),'entry_sp':str(sp),
             'entry_x':str(0xf0),'entry_a':str(8),'entry_p':'1'}
        return rom,row,sorted(changed),before,after

    def test_rom_specific_irq_signature(self):
        self.assertTrue(v.irq_stack_matches(*self.make_irq_case()))

    def test_rom_irq_vector_cannot_be_fabricated(self):
        rom,row,changed,before,after=self.make_irq_case()
        rom[v.BASE+0x3ffe]=0
        self.assertFalse(v.irq_stack_matches(rom,row,changed,before,after))

    def test_mutated_irq_stack_fails_closed(self):
        rom,row,changed,before,after=self.make_irq_case()
        after[0x1f0]^=1
        self.assertFalse(v.irq_stack_matches(rom,row,changed,before,after))

    def test_unrelated_caller_does_not_inherit_irq_proof(self):
        rom,row,changed,before,after=self.make_irq_case()
        row['caller_pc']=str(0x8429)
        self.assertFalse(v.irq_stack_matches(rom,row,changed,before,after))

    def test_unsigned_fields_reject_nondecimal(self):
        for bad in ('-1','12.0',' 12','0x20',''):
            with self.assertRaises(ValueError):
                v.unsigned(bad,'test',255)


if __name__=='__main__':
    unittest.main()
