#!/usr/bin/env python3
"""Fail-closed owner-private native collision ZIP audit using authentic JP ROM.

Original CPU continues to execute: success is ROM instruction-state and native
DMA accounting, NOT Mesen/NESRecomp paired game state, DMC hardware equivalence,
or authorization for native C substitution. No raw RAM is emitted.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import sys
from zipfile import ZipFile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests'/'references'))
import fcde_reference as reference
import verify_fcde_trace_gate_v11 as gate

DMA_FIELDS=('sample','caller_pc','entry_cycle','return_cycle',
            'instruction_cycles','dma_cycles','dma_halt_cycles',
            'dma_read_cycles','dma_write_cycles',
            'dmc_fetches_confirmed','oam_dma_writes')
ROM_SHA=reference.ROM_SHA
PINNED_ENGINE='2b4fcdb98c9e702be3fb365cfc645003c50b5fc1'
CAP=10000
FILE_CAP=96*1024*1024

def unsigned(row,key,number,maximum=(1<<64)-1):
    v=row.get(key)
    if v is None or not v.isascii() or not v.isdecimal():
        raise ValueError(f'sample {number}: {key} must be decimal')
    n=int(v)
    if n>maximum:raise ValueError(f'sample {number}: {key} out of range')
    return n

def parse_csv(data,fields,name):
    rd=csv.DictReader(io.StringIO(data.decode('utf-8-sig')),strict=True)
    if tuple(rd.fieldnames or ())!=tuple(fields):
        raise ValueError(f'{name}: schema mismatch')
    rows=list(rd)
    if not 1<=len(rows)<=CAP:
        raise ValueError(f'{name}: invalid capture row count {len(rows)}')
    if any(None in r or any(r.get(f) is None for f in fields) for r in rows):
        raise ValueError(f'{name}: truncated or unexpected data')
    return rows

def read_zip(path):
    needed=('native-fcde.csv','native-dma.csv','native-cycle-frame-IRQ-window.txt',
            'run-receipt.json','native-console.log','native-hashes.txt')
    with ZipFile(path) as z:
        if z.testzip() is not None:
            raise ValueError('capture ZIP failed CRC test')
        found={}
        for member in z.infolist():
            basename=member.filename.replace('\\','/').rsplit('/',1)[-1]
            if basename in needed:
                if basename in found:raise ValueError('duplicate file: '+basename)
                if member.file_size>FILE_CAP:raise ValueError('oversized file: '+basename)
                found[basename]=z.read(member)
    if set(found)!=set(needed):
        raise ValueError('missing owner native ZIP file(s): '+str(sorted(set(needed)-set(found))))
    return found

def rom_outcome(rom,row,i):
    old=bytes.fromhex(row['entry_ram_hex'])
    new=bytes.fromhex(row['return_ram_hex'])
    if len(old)!=2048 or len(new)!=2048:
        raise ValueError(f'sample {i}: 2KiB RAM shape mismatch')
    p=unsigned(row,'entry_p',i,255)
    expected=reference.run_collision(
        rom,old,
        a=unsigned(row,'entry_a',i,255),x=unsigned(row,'entry_x',i,255),
        y=unsigned(row,'entry_y',i,255),sp=unsigned(row,'entry_sp',i,255),
        carry=bool(p&1),zero=bool(p&2),negative=bool(p&128),
        overflow=bool(p&64))
    calculated=bytearray(old)
    for addr,delta in expected.ram_delta.items():
        calculated[int(addr,16)]=delta['new']
    regs=(unsigned(row,'return_a',i,255)==expected.a
          and unsigned(row,'return_x',i,255)==expected.x
          and unsigned(row,'return_y',i,255)==expected.y
          and unsigned(row,'return_sp',i,255)==((expected.sp+2)&255))
    flags=int(expected.carry)|(int(expected.zero)<<1)|(
           int(expected.overflow)<<6)|(int(expected.negative)<<7)
    status=(p&0x0C)|flags
    regflags=regs and (unsigned(row,'return_p',i,255)&0xCF)==(status&0xCF)
    return regflags,calculated==new,expected.cycles,set(expected.path),old

def key_for(row):
    # The NMOS status bit 5 is unobservable; preserve C,Z,I,D,V,N.
    return (row['caller_pc'],row['entry_a'],row['entry_x'],row['entry_y'],
            row['entry_sp'],int(row['entry_p'])&0xCF,row['entry_ram_hex'])

def verify(rom_path,zip_path,report_path,mesen_path=None):
    rom=Path(rom_path).read_bytes()
    if len(rom)!=393232 or hashlib.sha256(rom).hexdigest()!=ROM_SHA:
        raise ValueError('wrong Japanese ROM size/SHA256')
    data=read_zip(zip_path)
    native=parse_csv(data['native-fcde.csv'],gate.FIELDS,'native CPU')
    dma=parse_csv(data['native-dma.csv'],DMA_FIELDS,'native DMA')
    if len(native)!=len(dma):raise ValueError('CPU/DMA row count mismatch')
    receipt=json.loads(data['run-receipt.json'].decode('utf-8-sig'))
    if (receipt.get('rom_sha256')!=ROM_SHA
        or receipt.get('source_engine_commit')!=PINNED_ENGINE
        or receipt.get('native_cpu_samples')!=len(native)
        or receipt.get('native_dma_samples')!=len(dma)
        or receipt.get('observed_native_executed') is not True):
        raise ValueError('unverified ROM/engine/row count in native receipt')
    counts=Counter()
    executed=set()
    previous_return=-1
    failed=[]
    for i,(row,d) in enumerate(zip(native,dma),1):
        issues=[]
        caller=unsigned(row,'caller_pc',i,65535)
        if caller not in (0x8345,0x8429,0x96E4,0x96ED):
            raise ValueError(f'sample {i}: caller not among opt-in hooks')
        pos=0x38010+caller-0x8000
        if rom[pos:pos+3]!=b'\x20\xde\xfc':
            raise ValueError(f'sample {i}: original JSR differs')
        start=unsigned(row,'entry_cycle',i)
        end=unsigned(row,'return_cycle',i)
        if start<=previous_return or end<=start:
            issues.append('nonmonotone/colliding original CPU interval')
        previous_return=end
        reg,ram,cycles,paths,entry_ram=rom_outcome(rom,row,i)
        executed.update(paths)
        sp=unsigned(row,'entry_sp',i,255)
        if entry_ram[0x23]!=14:
            issues.append('wrong mapped PRG bank selector')
        stack=entry_ram[0x100+((sp+1)&255)]|(
              entry_ram[0x100+((sp+2)&255)]<<8)
        if stack!=(caller+2)&65535:
            issues.append('original return address on JSR stack differs')
        if not reg:issues.append('ROM return registers/status mismatch')
        if not ram:issues.append('ROM return full 2KiB RAM mismatch')
        if unsigned(d,'sample',i,CAP)!=i or unsigned(d,'caller_pc',i,65535)!=caller:
            issues.append('DMA sample order/caller mismatch')
        if unsigned(d,'entry_cycle',i)!=start or unsigned(d,'return_cycle',i)!=end:
            issues.append('DMA entry/return clock mismatch')
        dma_cycles=unsigned(d,'dma_cycles',i)
        halt=unsigned(d,'dma_halt_cycles',i)
        reads=unsigned(d,'dma_read_cycles',i)
        writes=unsigned(d,'dma_write_cycles',i)
        if halt+reads+writes!=dma_cycles:issues.append('DMA cycle sum mismatch')
        if unsigned(d,'dmc_fetches_confirmed',i)>reads:
            issues.append('DMC count exceeds DMA reads')
        if unsigned(d,'oam_dma_writes',i)>writes:
            issues.append('OAM write count exceeds DMA writes')
        if unsigned(d,'instruction_cycles',i)!=cycles:
            issues.append('native C prediction != ROM opcode instruction clocks')
        if end-start!=cycles+dma_cycles:
            issues.append('elapsed clocks != ROM instructions plus real DMA')
        counts[f'0x{caller:04X}']+=1
        counts['dma_'+str(dma_cycles)]+=1
        if issues:failed.append({'sample':i,'issues':issues})
    if failed:raise ValueError('native capture fails ROM/DMA audit: '+json.dumps(failed[:5]))
    cross={'mesen_source_rows':0,'exact_shared_caller_CPU_entry_and_2k_ram_pairs':None,
           'close_clock_same_caller_Mesen_rows_within_100_cycles':None,
           'cross_emulator_session_alignment_proven':False}
    if mesen_path is not None:
        gate.parse_rows(Path(mesen_path),rom)
        with open(mesen_path,newline='',encoding='utf-8-sig') as f:
            mesen=list(csv.DictReader(f,strict=True))
        index=defaultdict(list)
        for j,row in enumerate(native,1):index[key_for(row)].append(j)
        matches=sum(len(index.get(key_for(row),())) for row in mesen)
        nearby=0
        for row in mesen:
            t=int(row['entry_cycle'])
            if any(other['caller_pc']==row['caller_pc'] and
                   abs(int(other['entry_cycle'])-t)<100 for other in native):
                nearby+=1
        cross.update(mesen_source_rows=len(mesen),
                     exact_shared_caller_CPU_entry_and_2k_ram_pairs=matches,
                     close_clock_same_caller_Mesen_rows_within_100_cycles=nearby)
    trace=data['native-cycle-frame-IRQ-window.txt'].decode('utf-8',errors='strict')
    irq_cycles=[int(m.group(1)) for m in re.finditer(
                r'(?m)^(\d+) INSN PC=E0F5\b',trace)]
    overlaps=sum(any(int(row['entry_cycle'])<=clock<int(row['return_cycle'])
                     for clock in irq_cycles) for row in native)
    result={'schema':'cv3dcjp-jp-collision-v11-native-rom-original-cpu-dma-audit',
       'source_zip_sha256':hashlib.sha256(Path(zip_path).read_bytes()).hexdigest(),
       'rom_sha256':ROM_SHA,'pinned_engine':PINNED_ENGINE,
       'native_capture_samples':len(native),'matched_cpu_dma_row_boundaries':len(native),
       'original_rom_callsite_stack_bank_checks_passed':len(native),
       'ROM_register_flag_and_complete_RAM_matches':len(native),
       'ROM_instruction_cycles_equal_C_prediction':len(native),
       'ROM_instructions_plus_native_DMA_equal_elapsed':len(native),
       'original_ROM_executed_instruction_addresses':len(executed),
       'caller_counts':{k:v for k,v in counts.items() if k.startswith('0x')},
       'dma_cycle_histogram':{k[4:]:v for k,v in counts.items() if k.startswith('dma_')},
       'total_DMC_fetches_confirmed':sum(int(d['dmc_fetches_confirmed']) for d in dma),
       'native_irq_handler_E0F5_cpu_cycles_within_one_traced_frame':irq_cycles,
       'native_observed_FCDE_calls_overlap_traced_IRQ':overlaps,
       'cross_emulator':cross,
       'native_substitution_enabled':False,'CPU_DMC_IRQ_bus_parity_proven':False,
       'retired_original_6502_bytes':0,
       'private_capture_and_RAM_stored_in_Git':False}
    out=Path(report_path);out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for x in ('rom','zip','report'):p.add_argument('--'+x,required=True)
    p.add_argument('--mesen')
    a=p.parse_args()
    r=verify(a.rom,a.zip,a.report,a.mesen)
    print('Native ROM collision:'+str(r['native_capture_samples'])+
          ' calls; CPU/RAM/instruction+DMA all exact; cross-engine session NOT proven.')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
