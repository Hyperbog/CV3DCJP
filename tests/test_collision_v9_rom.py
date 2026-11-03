"""ROM-required differential acceptance for canonical collision code (v9).

Usage: CV3DCJP_JP_ROM=/path/to/Japanese.nes python -m unittest
       discover -s tests -p test_collision_v9_rom.py -v
Without a private ROM this test is skipped; no copyrighted ROM bytes are shipped.
"""
import ctypes as ct
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
REFS = ROOT / 'tests' / 'references'
sys.path.insert(0, str(REFS))
HAS_ROM = bool(os.environ.get('CV3DCJP_JP_ROM') and Path(os.environ['CV3DCJP_JP_ROM']).is_file())

class NativeState(ct.Structure):
    _fields_ = [('a',ct.c_uint8),('x',ct.c_uint8),('y',ct.c_uint8),('sp',ct.c_uint8),
                ('p',ct.c_uint8),('ram',ct.c_uint8*2048),
                ('mapped_prg_bank',ct.c_uint8),('mapper_writes',ct.c_uint)]

class NativeExit(ct.Structure):
    _fields_ = [('next_pc',ct.c_uint16),('kind',ct.c_int),('instructions',ct.c_uint),
                ('ok',ct.c_uint8),('carry',ct.c_uint8)]

CALLS = [0x8345,0x834e,0x83ea,0x83f6,0x8429,0x8450,0x84cf,0x84d8,
         0x9153,0x9164,0x97b1,0x97ba,0x97c5,0x97ce,0x97e7,0x97fc]
QUERIES = {0x834e,0x8450,0x84d8,0x97ba,0x97ce,0x97fc}
CALL_BOUNDARIES = {0x8353,0x8359,0x84df,0x84e8,0x97d3,0x97db,0x97f0,0x9805,0x981f}
RETURNS = {0x83f5,0x83fd,0x8478,0x847a,0x9159,0x9176,0x9847}
JUMPS = {0x9848}
UNTRANSLATED = {0x84ab,0x847b,0x8495,0x83ca,0x83fe,0x83da,0x84b4,0x915e}
BOUNDARIES = QUERIES | CALL_BOUNDARIES | RETURNS | JUMPS | UNTRANSLATED

def to_native(m):
    s=NativeState();s.a=m.cpu.a;s.x=m.cpu.x;s.y=m.cpu.y;s.p=m.cpu.p;s.sp=m.sp
    s.ram[:]=bytes(m.cpu.ram);s.mapped_prg_bank=m.mapped_prg_bank;s.mapper_writes=m.mapper_writes
    return s

def compare_state(test,s,r):
    test.assertEqual((s.a,s.x,s.y,s.p,s.sp,s.mapped_prg_bank,s.mapper_writes),
                     (r.a,r.x,r.y,r.p,r.sp,r.bank,r.writes))
    test.assertEqual(bytes(s.ram),bytes(r.ram))

def caller_ref(v,m,entry):
    r=v.Ref(m,entry+3)
    for _ in range(100):
        if r.pc in BOUNDARIES:
            p=r.pc
            return r, (4 if p in QUERIES else 2 if p in CALL_BOUNDARIES else
                       1 if p in RETURNS else 3 if p in JUMPS else 5)
        r.step()
    raise AssertionError(f'unresolved bounded path: ${entry:04X} -> ${r.pc:04X}')

@unittest.skipUnless(HAS_ROM, 'Owner-local CV3DCJP_JP_ROM is required')
class CollisionV9ROM(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import rom_reference as v
        from disasm6502 import cfg
        cls.v=v;cls.temp=tempfile.TemporaryDirectory()
        so=Path(cls.temp.name)/'collision.so'
        cmd=['cc','-std=c11','-O2','-fPIC','-shared','-Wall','-Wextra','-Werror',
             '-Wno-unused-function',str(ROOT/'src/collision/Collision.c'),'-o',str(so)]
        subprocess.run(cmd,check=True,capture_output=True)
        lib=ct.CDLL(str(so))
        cls.caller=lib.Collision_Caller
        cls.caller.argtypes=[ct.c_uint16,ct.POINTER(NativeState)]
        cls.caller.restype=NativeExit
        cls.external=lib.Collision_External
        cls.external.argtypes=[ct.c_uint16,ct.POINTER(NativeState),ct.c_void_p,ct.c_size_t]
        cls.external.restype=NativeExit

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_all_16_bounded_caller_continuations(self):
        v=self.v;rng=random.Random(0xC0111519)
        observed=set();branches=set()
        for j,call in enumerate(CALLS):
            exits=set()
            for i in range(500):
                m=v.seed(rng,i+1000*j,'86fb')
                m.cpu.p=(m.cpu.p&~2)|(2 if i%2==0 else 0)
                m.cpu.a=[0,1,2,3,4,5,6,7,8,9,11,12,13,14,15,255][(i//2)%16]
                m.cpu.ram[0x88]=[0,1,2,5,6,7,255][i%7]
                m.cpu.ram[0x01]=(i*7)&255
                m.cpu.ram[0x7f]=i&255
                ref,kind=caller_ref(v,m,call)
                native=to_native(m);result=self.caller(call,ct.byref(native))
                self.assertEqual((result.ok,result.next_pc,result.kind),(1,ref.pc,kind),(hex(call),i))
                compare_state(self,native,ref)
                exits.add(result.next_pc);observed.update(ref.cover);branches.update(ref.edges)
            self.assertTrue(exits)
        self.assertEqual(len(observed),102)
        self.assertEqual(len(branches),75)

    def test_86fb_branch_closure(self):
        v=self.v
        from disasm6502 import cfg
        graph,errors,_,_=cfg(v.ROM,0x86fb,0x869e,0x8713,
                                  cpu_base=0x8000,rom_base=16+14*0x4000)
        self.assertEqual(len(graph),55);self.assertFalse(errors)
        rng=random.Random(0x86fbef91)
        inputs=[v.seed(rng,i,'86fb') for i in range(5000)]
        rows=[(0x80,0,0,0,0,0,0,0),(0x81,1,0,0,2,0,0,0),
              (0x81,1,0,0x80,0,0x11,0x11,0x30),
              (0x81,1,0,0x80,0,0x11,0x11,1),
              (0x81,1,0,0x80,0,0x11,0x12,0x30),
              (0x80,0x80,0,0x80,0,0x11,0x11,0x30),
              (0x80,0x80,0,0,0,0x11,0x11,0x30)]
        for i,(r65,r520,r537,r41c,r53,r54,r6e,r53b) in enumerate(rows):
            m=v.seed(random.Random(1000+i),i,'86fb')
            for addr,val in {0x65:r65,0x520:r520,0x537:r537,0x41c:r41c,
                             0x53:r53b if i>=2 else r53,0x54:r54,0x6e:r6e}.items():
                m.cpu.ram[addr]=val
            inputs.append(m)
        covered=set();edges=set()
        for m in inputs:
            ref=v.Ref(m,0x86fb);ref.run()
            native=to_native(m);result=self.external(0x86fb,ct.byref(native),None,0)
            self.assertEqual(result.ok,1)
            compare_state(self,native,ref)
            covered.update(pc for bank,pc in ref.cover if bank==14)
            edges.update((pc,t) for bank,pc,t in ref.edges if bank==14)
        self.assertEqual(covered,{ins.pc for ins in graph})
        expected={(ins.pc,t) for ins in graph if ins.mode=='rel' for t in (True,False)}
        self.assertEqual(expected-edges,{(0x86aa,False)})

if __name__=='__main__':unittest.main()
