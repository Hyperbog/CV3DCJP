#!/usr/bin/env python3
"""Verify owner MesenCE v11.2 collision/DMC/controller ZIP against JP ROM.

Original ROM is mandatory. No input data, ROM bytes, or RAM snapshots are
committed or placed in the output report. This is NOT per-bus-cycle parity.
"""
import argparse
from collections import Counter
import csv
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import sys
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests'/'references'))
sys.path.insert(0,str(ROOT/'tools'/'instrumentation'))
import fcde_reference as reference
import verify_fcde_trace_gate_v11 as gate

PREFIXES={
    'collision':'cv3dcjp-fcde-sync-v11-',
    'dmc':'cv3dcjp-fcde-dmc-v11-',
    'controller':'cv3dcjp-controller-v11-'
}
DMC_FIELDS=('sample','caller_pc','entry_cycle','return_cycle',
            'entry_dmc_address','return_dmc_address',
            'entry_dmc_remaining','return_dmc_remaining',
            'entry_buffer_empty','return_buffer_empty')
INPUT_FIELDS=('reset_epoch','mesen_frame','cycle','port1_mask','port2_mask')
BASE=0x3c010
IRQ_PROLOGUE=bytes.fromhex('48 8a 48 98 48 a5 00 48 a5 01 48')


def unsigned(value, label, maximum=(1<<64)-1):
    if not value or not value.isascii() or not value.isdecimal():
        raise ValueError(f'{label}: not a decimal integer')
    n=int(value)
    if n>maximum: raise ValueError(f'{label}: out of range')
    return n


def parse_csv(data, fields, source):
    text=data.decode('utf-8-sig')
    reader=csv.DictReader(io.StringIO(text,newline=''),strict=True)
    if tuple(reader.fieldnames or ())!=tuple(fields):
        raise ValueError(f'{source}: CSV schema mismatch')
    records=list(reader)
    if not records or any(None in r or any(r.get(f) is None for f in fields)
                          for r in records):
        raise ValueError(f'{source}: missing/extra data')
    return records


def read_zip(path):
    results={}
    hashes={}
    with zipfile.ZipFile(path) as archive:
        for obj in archive.infolist():
            name=PurePosixPath(obj.filename).name
            for key,prefix in PREFIXES.items():
                if name.startswith(prefix) and name.endswith('.csv'):
                    if key in results: raise ValueError('duplicate '+key+' capture')
                    data=archive.read(obj)
                    if len(data)>5_000_000:
                        raise ValueError(key+' capture exceeds limit')
                    results[key]=data
                    hashes[key]=hashlib.sha256(data).hexdigest()
    if set(results)!=set(PREFIXES): raise ValueError('expected three v11.2 CSVs')
    stems=[PurePosixPath(k).name for k in PREFIXES.values()]
    return results,hashes


def rom_cycles_and_state(rom, row):
    p=unsigned(row['entry_p'],'entry_p',255)
    outcome=reference.run_collision(
        rom,bytes.fromhex(row['entry_ram_hex']),
        a=unsigned(row['entry_a'],'entry_a',255),
        x=unsigned(row['entry_x'],'entry_x',255),
        y=unsigned(row['entry_y'],'entry_y',255),
        sp=unsigned(row['entry_sp'],'entry_sp',255),
        carry=bool(p&1),zero=bool(p&2),
        negative=bool(p&0x80),overflow=bool(p&0x40))
    before=bytes.fromhex(row['entry_ram_hex'])
    predicted=bytearray(before)
    for addr,change in outcome.ram_delta.items():
        predicted[int(addr,16)]=change['new']
    actual=bytes.fromhex(row['return_ram_hex'])
    reg_ok=(outcome.a==int(row['return_a']) and
            outcome.x==int(row['return_x']) and
            outcome.y==int(row['return_y']) and
            ((outcome.sp+2)&255)==int(row['return_sp']))
    flags=(p&0x3c)|(int(outcome.carry))|(int(outcome.zero)<<1)|\
          (int(outcome.overflow)<<6)|(int(outcome.negative)<<7)
    status_ok=(flags&0xcf)==(int(row['return_p'])&0xcf)
    changed=[a for a in range(2048) if predicted[a]!=actual[a]]
    return outcome.cycles,reg_ok,status_ok,changed,before,actual


def irq_stack_matches(rom,row,changed,before,actual):
    """Classify ONLY the observed IRQ signature; never normalize arbitrary RAM."""
    pc_vector=int.from_bytes(rom[BASE+0x3ffe:BASE+0x4000],'little')
    if pc_vector!=0xe0f5 or rom[BASE+pc_vector-0xc000:
                                BASE+pc_vector-0xc000+11]!=IRQ_PROLOGUE:
        return False
    # At original $FCEB in the horizontal path, PLA has restored entry SP.
    sp=int(row['entry_sp'])
    x=int(row['entry_x'])
    y=(int(row['entry_a'])+before[0x438])&255
    saved_status=(int(row['entry_p'])&0x4c)|0x20|(x&0x80)
    values=(0xfc,0xeb,saved_status,x,x,y,before[0],before[1])
    addrs=[0x100+((sp-i)&255) for i in range(8)]
    return (sorted(changed)==sorted(addrs) and
            all(actual[a]==v for a,v in zip(addrs,values)) and
            int(row['caller_pc'])==0x96ed)


def verify(rom_path,zip_path,report_path,residual_path=None):
    rom=Path(rom_path).read_bytes()
    if len(rom)!=393232 or hashlib.sha256(rom).hexdigest()!=reference.ROM_SHA:
        raise ValueError('incorrect original Japanese ROM')
    data,hashes=read_zip(zip_path)
    fields={'collision':gate.FIELDS,'dmc':DMC_FIELDS,'controller':INPUT_FIELDS}
    rec={k:parse_csv(data[k],fields[k],k) for k in PREFIXES}
    if len(rec['collision'])!=len(rec['dmc']) or len(rec['collision'])>128:
        raise ValueError('unpaired or excessive collision/DMC rows')
    with tempfile.TemporaryDirectory(prefix='cv3dcjp-v11-') as temp:
        trace=Path(temp)/'original.csv'
        trace.write_bytes(data['collision'])
        gate.parse_rows(trace,rom)
    callers=Counter();residuals=Counter();transitions=Counter()
    classifications=Counter();failures=[];details=[]
    for n,(row,dmc) in enumerate(zip(rec['collision'],rec['dmc']),1):
        issues=[]
        if unsigned(dmc['sample'],'sample',128)!=n: issues.append('index')
        if any(row[k]!=dmc[k] for k in
               ('caller_pc','entry_cycle','return_cycle')):
            issues.append('DMC boundary misalignment')
        cycles,regs,flags,ram_diff,before,after=rom_cycles_and_state(rom,row)
        elapsed=int(row['return_cycle'])-int(row['entry_cycle'])
        extra=elapsed-cycles
        da=int(dmc['return_dmc_address'])-int(dmc['entry_dmc_address'])
        dl=int(dmc['return_dmc_remaining'])-int(dmc['entry_dmc_remaining'])
        if any(unsigned(dmc[k],k,65535)>1 for k in
               ('entry_buffer_empty','return_buffer_empty')):
            issues.append('invalid DMC buffer status')
        if not regs or not flags: issues.append('CPU return register/flag mismatch')
        if (da,dl) not in ((0,0),(1,-1)):
            issues.append('unclassified DMC reader transition')
        irq=False
        if extra==0 and not ram_diff and da==0 and dl==0:
            kind='exact'
        elif extra==4 and not ram_diff and (da,dl)==(1,-1):
            kind='DMC-four-cycle-correlated'
        elif extra==246 and (da,dl)==(1,-1) and irq_stack_matches(
                 rom,row,ram_diff,before,after):
            kind='DMC-and-ROM-IRQ-stack'
            irq=True
        else:
            kind='unclassified'
            issues.append('unexpected clock/RAM/DMC combination')
        callers[f"0x{int(row['caller_pc']):04X}"]+=1
        residuals[str(extra)]+=1
        transitions[f'{da:+d}/{dl:+d}']+=1
        classifications[kind]+=1
        if issues: failures.append({'sample':n,'fields':issues})
        details.append({'sample':n,'caller':f"0x{int(row['caller_pc']):04X}",
                        'rom_cycles':cycles,'observed_cycles':elapsed,
                        'residual':extra,'dmc_advance':da,
                        'dmc_remaining_delta':dl,'irq_stack':irq,
                        'classification':kind})
    input_by_epoch={}
    for inp in rec['controller']:
        epoch=unsigned(inp['reset_epoch'],'reset_epoch',65535)
        frame=unsigned(inp['mesen_frame'],'mesen_frame')
        unsigned(inp['cycle'],'cycle')
        unsigned(inp['port1_mask'],'port1_mask',255)
        unsigned(inp['port2_mask'],'port2_mask',255)
        input_by_epoch.setdefault(epoch,[]).append(frame)
    seq=input_by_epoch.get(1,[])
    contiguous=(bool(seq) and all(b==a+1 for a,b in zip(seq,seq[1:])))
    result={
       'schema':'cv3dcjp-fcde-sync-dmc-irq-v11.2',
       'rom_sha256':reference.ROM_SHA,
       'capture_zip_sha256':hashlib.sha256(Path(zip_path).read_bytes()).hexdigest(),
       'csv_sha256':hashes,'strict_original_rom_preflight':True,
       'samples':len(details),'callers':dict(callers),
       'cycle_residuals':dict(residuals),
       'dmc_address_and_remaining_changes':dict(transitions),
       'classifications':dict(classifications),
       'controller_input_poll_rows':len(rec['controller']),
       'reset_epoch_counts':{str(k):len(v) for k,v in input_by_epoch.items()},
       'post_reset_epoch_1_frame_sequence_contiguous':contiguous,
       'irq_vector':'0xE0F5' if classifications['DMC-and-ROM-IRQ-stack'] else None,
       'irq_return_pc':'0xFCEB' if classifications['DMC-and-ROM-IRQ-stack'] else None,
       'mismatches':failures,
       'engine_provenance_verified':False,
       'paired_nesrecomp_capture_available':False,
       'per_bus_cycle_dmc_parity_verified':False,
       'native_substitution_authorized':False,
       'retired_prg_bytes':0
    }
    path=Path(report_path);path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    if residual_path:
        with open(residual_path,'w',newline='',encoding='utf-8') as f:
            writer=csv.DictWriter(f,fieldnames=details[0].keys())
            writer.writeheader();writer.writerows(details)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',required=True)
    p.add_argument('--zip',required=True)
    p.add_argument('--report',required=True)
    p.add_argument('--residuals-csv')
    a=p.parse_args()
    r=verify(a.rom,a.zip,a.report,a.residuals_csv)
    print(f"FCDE sync: {r['samples']} records; "+str(r['classifications'])+
          f"; unclassified={len(r['mismatches'])}")
    return int(bool(r['mismatches']))


if __name__=='__main__':
    raise SystemExit(main())
