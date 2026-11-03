"""Synthetic CSV contract test; NOT a substitute for real Mesen2 execution."""
import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'instrumentation'))
import verify_fcde_live_trace_v10 as v

class TraceParser(unittest.TestCase):
    @unittest.skipUnless(os.environ.get('CV3DCJP_JP_ROM') and
                         Path(os.environ['CV3DCJP_JP_ROM']).is_file(),
                         'Owner ROM fixture unavailable')
    def test_synthetic_row_then_corruption(self):
        rom_path=Path(os.environ['CV3DCJP_JP_ROM'])
        rom=rom_path.read_bytes()
        with tempfile.TemporaryDirectory() as folder:
            import ctypes as ct
            so=Path(folder)/'test.so'
            subprocess.run(['cc','-std=c11','-O2','-fPIC','-shared',
                            '-Wall','-Wextra','-Werror','-Wno-unused-function',
                            str(ROOT/'src/collision/Collision.c'),
                            str(ROOT/'src/collision/CollisionLive.c'),'-o',str(so)],check=True)
            native=ct.CDLL(str(so)).CollisionLive_ProbeFCDE
            native.argtypes=[ct.POINTER(v.Snapshot),ct.c_uint16,
                             ct.POINTER(ct.c_uint8),ct.c_size_t,ct.POINTER(v.Preview)]
            native.restype=ct.c_int
            s=v.Snapshot();s.pc=0xfcde;s.cpu_cycles=1000
            s.cpu.mapped_prg_bank=14;s.cpu.ram[0x23]=14
            s.cpu.a=5;s.cpu.x=0x30;s.cpu.sp=0xfb
            s.cpu.ram[0x1fc]=0x47;s.cpu.ram[0x1fd]=0x83
            s.cpu.ram[0x438]=0x20;s.cpu.ram[0x41c]=0x80;s.cpu.ram[0x53]=0x13
            rom_buffer=(ct.c_uint8*len(rom)).from_buffer_copy(rom)
            out=v.Preview()
            self.assertEqual(native(ct.byref(s),0x8345,rom_buffer,len(rom),ct.byref(out)),0)
            row={'caller_pc':0x8345,'entry_cycle':s.cpu_cycles,
                 'return_cycle':out.after.cpu_cycles,
                 'entry_ram_hex':bytes(s.cpu.ram).hex(),
                 'return_ram_hex':bytes(out.after.cpu.ram).hex()}
            for field in ('a','x','y','sp','p'):
                row['entry_'+field]=getattr(s.cpu,field)
                row['return_'+field]=getattr(out.after.cpu,field)
            csv_path=Path(folder)/'trace.csv';report_path=Path(folder)/'report.json'
            def write():
                with csv_path.open('w',newline='') as file:
                    w=csv.DictWriter(file,fieldnames=list(row));w.writeheader();w.writerow(row)
            write()
            report=v.verify(rom_path,csv_path,report_path)
            self.assertEqual((report['matches'],report['mismatches']),(1,0))
            self.assertFalse(report['live_substitution_executed'])
            row['return_cycle']+=1;write()
            report=v.verify(rom_path,csv_path,report_path)
            self.assertEqual((report['matches'],report['mismatches']),(0,1))
            self.assertEqual(report['results'][0]['differences'],['cpu_cycles'])
            row['return_cycle']-=1;row['return_ram_hex']='00'*2048;write()
            report=v.verify(rom_path,csv_path,report_path)
            self.assertIn('ram_2k',report['results'][0]['differences'])

if __name__=='__main__':unittest.main()
