"""6502 CPU-clock parity for original Japanese $FCDE query (offline, ROM required).

The original interpreter executes authenticated opcodes; C uses handwritten
native control flow with separate per-operation cycle annotations. This checks
CPU cycle totals, not individual bus transactions, DMA, IRQ/NMI or PPU phases.
"""
import ctypes as ct
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'/'references'))
ROM_PATH=os.environ.get('CV3DCJP_JP_ROM')

class NativeState(ct.Structure):
    _fields_=[('a',ct.c_uint8),('x',ct.c_uint8),('y',ct.c_uint8),
              ('sp',ct.c_uint8),('p',ct.c_uint8),('ram',ct.c_uint8*2048),
              ('mapped_prg_bank',ct.c_uint8),('mapper_writes',ct.c_uint)]
class NativeExit(ct.Structure):
    _fields_=[('next_pc',ct.c_uint16),('kind',ct.c_int),
              ('instructions',ct.c_uint),('ok',ct.c_uint8),('carry',ct.c_uint8)]
class TimedExit(ct.Structure):
    _fields_=[('exit',NativeExit),('cpu_cycles',ct.c_uint)]

@unittest.skipUnless(ROM_PATH and Path(ROM_PATH).is_file(),'Authenticated Japanese ROM not supplied')
class FCDEClock(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import fcde_reference
        cls.reference=fcde_reference
        cls.rom=Path(ROM_PATH).read_bytes()
        cls.bank=(ct.c_uint8*16384).from_buffer_copy(cls.rom[0x3c010:0x40010])
        cls.tmp=tempfile.TemporaryDirectory()
        library=Path(cls.tmp.name)/'collision.so'
        subprocess.run(['cc','-std=c11','-O2','-shared','-fPIC',
                        '-Wall','-Wextra','-Werror','-Wno-unused-function',
                        str(ROOT/'src/collision/Collision.c'),'-o',str(library)],check=True)
        cls.native=ct.CDLL(str(library)).Collision_FCDE_Timed
        cls.native.argtypes=[ct.POINTER(NativeState),ct.POINTER(ct.c_uint8)]
        cls.native.restype=TimedExit
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()

    def test_full_register_ram_clock_comparison(self):
        r=random.Random(0x6502fcde)
        seen=set();cycles=set();exits=set()
        for k in range(2200):
            s=NativeState();s.a=r.randrange(256);s.x=r.randrange(256)
            s.y=r.randrange(256);s.sp=r.randrange(256);s.p=r.randrange(256)
            raw=bytearray(r.randbytes(2048))
            raw[0x65]=[0x00,0x01,0x7f,0x80,0x81,0xff][k%6]
            raw[0xcd]=[0,32,64,128,255][k%5]
            raw[0x41c]=[0,0x20,0x7f,0xc0,0xfe,0xff][(k//6)%6]
            raw[0x438]=[0,0x10,0x7f,0xf0,0xff][(k//36)%5]
            s.ram[:]=raw
            initial=bytes(raw)
            base=(s.a,s.x,s.y,s.sp,s.p)
            out=self.native(ct.byref(s),self.bank)
            reference=self.reference.run_collision(self.rom,initial,
                a=base[0],x=base[1],y=base[2],sp=base[3],
                carry=bool(base[4]&1),zero=bool(base[4]&2),
                negative=bool(base[4]&0x80),overflow=bool(base[4]&0x40))
            self.assertTrue(out.exit.ok,(k,base))
            self.assertEqual((out.cpu_cycles,out.exit.instructions),
                             (reference.cycles,reference.steps),(k,base,reference.path))
            self.assertEqual((s.a,s.x,s.y,s.sp),
                             (reference.a,reference.x,reference.y,reference.sp),k)
            self.assertEqual((s.p&0xc3),
                             (int(reference.carry)|int(reference.zero)<<1|
                              int(reference.overflow)<<6|int(reference.negative)<<7),k)
            for addr in range(2048):
                expected=reference.ram_delta.get(f'0x{addr:04x}')
                self.assertEqual(s.ram[addr],initial[addr] if expected is None else expected['new'],(k,addr))
            seen.update(reference.path);cycles.add(out.cpu_cycles);exits.add(out.exit.next_pc)
        self.assertEqual(len(seen),121)
        self.assertGreater(len(cycles),10)
        self.assertGreaterEqual(len(exits),4)

    def test_wrong_bank_rejects_unchanged(self):
        s=NativeState();s.sp=0xff;s.ram[0x65]=0x80
        before=bytes(s)
        bad=(ct.c_uint8*16384)()
        result=self.native(ct.byref(s),bad)
        self.assertFalse(result.exit.ok)
        self.assertEqual(result.cpu_cycles,0)
        self.assertEqual(bytes(s),before)

if __name__=='__main__':unittest.main()
