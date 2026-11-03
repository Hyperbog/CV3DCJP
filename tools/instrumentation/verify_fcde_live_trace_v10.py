#!/usr/bin/env python3
"""Compare Mesen2 real $FCDE entry/return observations against native C preview.
A capture is REQUIRED. This script never launches a game or changes its CPU.
"""
import argparse
import csv
import ctypes as ct
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

SHA='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
ROOT=Path(__file__).resolve().parents[2]

class Cpu(ct.Structure):
    _fields_=[('a',ct.c_uint8),('x',ct.c_uint8),('y',ct.c_uint8),
              ('sp',ct.c_uint8),('p',ct.c_uint8),('ram',ct.c_uint8*2048),
              ('mapped_prg_bank',ct.c_uint8),('mapper_writes',ct.c_uint)]
class Snapshot(ct.Structure):
    _fields_=[('cpu',Cpu),('pc',ct.c_uint16),('cpu_cycles',ct.c_uint64),
              ('interrupt_pending',ct.c_uint8),('dma_active',ct.c_uint8),
              ('mapper_irq_pending',ct.c_uint8)]
class Exit(ct.Structure):
    _fields_=[('next_pc',ct.c_uint16),('kind',ct.c_int),
              ('instructions',ct.c_uint),('ok',ct.c_uint8),('carry',ct.c_uint8)]
class TimedExit(ct.Structure):
    _fields_=[('exit',Exit),('cpu_cycles',ct.c_uint)]
class Preview(ct.Structure):
    _fields_=[('after',Snapshot),('query',TimedExit),('caller_pc',ct.c_uint16),
              ('status',ct.c_int),('runtime_commit_authorized',ct.c_uint8)]

def verify(rom_path,trace_path,out_path):
    rom=Path(rom_path).read_bytes()
    if len(rom)!=393232 or hashlib.sha256(rom).hexdigest()!=SHA:
        raise ValueError('Wrong Japanese original ROM (size/SHA256)')
    rom_buffer=(ct.c_uint8*len(rom)).from_buffer_copy(rom)
    results=[]
    with tempfile.TemporaryDirectory() as temp:
        library=Path(temp)/'collision_live.so'
        subprocess.run(['cc','-std=c11','-O2','-shared','-fPIC',
                        '-Wall','-Wextra','-Werror','-Wno-unused-function',
                        str(ROOT/'src/collision/Collision.c'),
                        str(ROOT/'src/collision/CollisionLive.c'),
                        '-o',str(library)],check=True)
        lib=ct.CDLL(str(library))
        func=lib.CollisionLive_ProbeFCDE
        func.argtypes=[ct.POINTER(Snapshot),ct.c_uint16,
                       ct.POINTER(ct.c_uint8),ct.c_size_t,ct.POINTER(Preview)]
        func.restype=ct.c_int
        with open(trace_path,newline='',encoding='utf-8') as file:
            for lineno,row in enumerate(csv.DictReader(file),2):
                s=Snapshot()
                caller=int(row['caller_pc'])
                s.pc=0xfcde;s.cpu_cycles=int(row['entry_cycle'])
                for field in ('a','x','y','sp','p'):
                    setattr(s.cpu,field,int(row['entry_'+field]))
                before=bytes.fromhex(row['entry_ram_hex'])
                after=bytes.fromhex(row['return_ram_hex'])
                if len(before)!=2048 or len(after)!=2048:
                    raise ValueError(f'Line {lineno}: RAM must contain 2048 bytes')
                s.cpu.ram[:]=before
                s.cpu.mapped_prg_bank=before[0x23]
                s.cpu.mapper_writes=0
                out=Preview()
                status=func(ct.byref(s),caller,rom_buffer,len(rom),ct.byref(out))
                diffs=[]
                if status!=0:diffs.append('probe_rejected:'+str(status))
                else:
                    expected_pc=caller+3
                    if out.after.pc!=expected_pc:diffs.append('return_pc')
                    if out.after.cpu_cycles!=int(row['return_cycle']):diffs.append('cpu_cycles')
                    for field in ('a','x','y','sp'):
                        if getattr(out.after.cpu,field)!=int(row['return_'+field]):
                            diffs.append(field)
                    # Ignore the 6502 status B/unused bits, not P's actual flags.
                    if (out.after.cpu.p&0xcf)!=(int(row['return_p'])&0xcf):
                        diffs.append('cpu_flags')
                    if bytes(out.after.cpu.ram)!=after:diffs.append('ram_2k')
                    if out.runtime_commit_authorized!=0:diffs.append('unsafe_commit')
                results.append({'line':lineno,'caller':f'${caller:04X}',
                                'observed_cycles':int(row['return_cycle'])-s.cpu_cycles,
                                'native_cycles':out.query.cpu_cycles if status==0 else None,
                                'equal':not diffs,'differences':diffs})
    report={'rom_sha256':SHA,'mode':'real Mesen2 entry/return vs offline native shadow',
            'live_substitution_executed':False,
            'bus_cycle_parity_verified':False,
            'samples':len(results),'matches':sum(z['equal'] for z in results),
            'mismatches':sum(not z['equal'] for z in results),'results':results}
    Path(out_path).parent.mkdir(parents=True,exist_ok=True)
    Path(out_path).write_text(json.dumps(report,indent=2)+'\n')
    if not results:raise RuntimeError('Empty trace; no parity evidence')
    return report

def main():
    a=argparse.ArgumentParser()
    a.add_argument('--rom',required=True)
    a.add_argument('--trace',required=True)
    a.add_argument('--report',required=True)
    args=a.parse_args()
    result=verify(args.rom,args.trace,args.report)
    print(f"Mesen2-native FCDE checks: {result['matches']}/{result['samples']} matched; live substitution NOT exercised")
    return int(bool(result['mismatches']))
if __name__=='__main__':raise SystemExit(main())
