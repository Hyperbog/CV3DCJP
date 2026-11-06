#!/usr/bin/env python3
"""Source-identified MesenCE vs pinned NESRecomp DMA bus gate for original JP $FCDE.

No invented clock offsets. No native C substitution. A 'DMC read match' is not
CPU/PPU/APU/IRQ or every-bus-cycle parity. Raw records stay owner-private.
"""
import argparse
import bisect
import csv
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests'/'references'))
import fcde_reference as reference
import verify_fcde_trace_gate_v11 as gate

M_FIELDS=('cpu_cycle','bus_kind','address','value','cpu_pc','real_dmc_sample_read')
N_FIELDS=('sample','caller_pc','cycle_index','kind','address','value','data_bus',
          'halt','dmc_address_before','dmc_address_after',
          'dmc_remaining_before','dmc_remaining_after',
          'dmc_fetch_verified','instruction_done')
D_FIELDS=('sample','caller_pc','entry_cycle','return_cycle','instruction_cycles',
          'dma_cycles','dma_halt_cycles','dma_read_cycles','dma_write_cycles',
          'dmc_fetches_confirmed','oam_dma_writes')
LIMIT=300000

def csv_rows(path,fields):
    with open(path,encoding='utf-8-sig',newline='') as f:
        rd=csv.DictReader(f,strict=True)
        if tuple(rd.fieldnames or ())!=fields:
            raise ValueError(f'Unexpected column schema in {path}')
        rows=list(rd)
    if not 1<=len(rows)<=LIMIT:
        raise ValueError(f'Empty/oversized CSV: {path}')
    for i,row in enumerate(rows,1):
        if None in row or any(row.get(k) is None for k in fields):
            raise ValueError(f'Truncated CSV in {path} row {i}')
    return rows

def number(row,name,maximum=(1<<64)-1):
    value=row[name]
    if not value.isascii() or not value.isdecimal():
        raise ValueError(f'Invalid integer: {name}')
    x=int(value)
    if x>maximum:raise ValueError(f'Out-of-range integer: {name}')
    return x

def real_mesen_dmc_rows(rows):
    result=[]
    previous=-1
    for row in rows:
        cycle=number(row,'cpu_cycle')
        if cycle<previous:raise ValueError('Mesen DMA log is not ordered')
        previous=cycle
        kind=row['bus_kind']
        dmc=number(row,'real_dmc_sample_read',1)
        addr=number(row,'address',65535)
        number(row,'cpu_pc',65535)
        number(row,'value',255)
        if kind not in ('R','W'):raise ValueError('Invalid Mesen DMA kind')
        if dmc and kind!='R':raise ValueError('DMC event is not a read')
        if dmc:result.append((cycle,addr))
    return result

def real_native_dmc_rows(rows):
    result=[]
    previous=-1
    for row in rows:
        sample=number(row,'sample',10000)
        cycle=number(row,'cycle_index')
        if cycle<previous:raise ValueError('Native DMA log is not ordered')
        previous=cycle
        addr=number(row,'address',65535)
        kind=row['kind']
        dmc=number(row,'dmc_fetch_verified',1)
        halt=number(row,'halt',1)
        if kind not in ('R','W','-'):
            raise ValueError('Invalid native bus kind')
        for field,limit in (('caller_pc',65535),('value',255),
             ('data_bus',255),('dmc_address_before',65535),
             ('dmc_address_after',65535),
             ('dmc_remaining_before',65535),
             ('dmc_remaining_after',65535),('instruction_done',1)):
            number(row,field,limit)
        if dmc:
            if kind!='R' or halt or number(row,'dmc_address_before',65535)!=addr:
                raise ValueError('Native DMC event lacks actual DMA read')
            result.append((sample,cycle,addr))
    return result

def in_window(rows,start,end):
    return [(cycle,addr) for cycle,addr in rows if start<=cycle<end]

def compare_events(mesen,native):
    """Only compare source-identified DMC fetches; never silently time-shift."""
    return {'same_exact_read_cycle_and_address':mesen==native,
            'mesen_reads':len(mesen),'nesrecomp_reads':len(native),
            'mesen_evidence':mesen,'native_evidence':native}

def verify(rom_path,mesen_cpu,mesen_bus,native_cpu,native_bus,native_dma,report_path):
    rom=Path(rom_path).read_bytes()
    if len(rom)!=393232 or hashlib.sha256(rom).hexdigest()!=reference.ROM_SHA:
        raise ValueError('Not the original Japanese ROM')
    gate.parse_rows(Path(mesen_cpu),rom)
    m=csv_rows(mesen_cpu,gate.FIELDS)
    n=csv_rows(native_cpu,gate.FIELDS)
    if len(m)!=128 or len(n)<128:
        raise ValueError('Require complete 128-row Mesen and >=128 native calls')
    mes_bus=csv_rows(mesen_bus,M_FIELDS)
    nat_bus=csv_rows(native_bus,N_FIELDS)
    dma=csv_rows(native_dma,D_FIELDS)
    if len(dma)!=len(n):
        raise ValueError('Native DMA boundary and CPU counts differ')
    mread=real_mesen_dmc_rows(mes_bus)
    nread=real_native_dmc_rows(nat_bus)
    n_by_sample={}
    for sample,cycle,addr in nread:
        n_by_sample.setdefault(sample,[]).append((cycle,addr))
    all_bus_samples={}
    for row in nat_bus:
        sample=number(row,'sample',10000)
        all_bus_samples[sample]=all_bus_samples.get(sample,0)+1
    differences=[];aggregate={'full_entry_RAM_equal':0,'CPU_registers_and_flags_equal':0,
        'equal_duration':0,'DMC_read_cycles_addresses_equal':0,'bus_dma_counts_equal':0}
    pairs=[]
    for i in range(128):
        row=m[i];native=n[i];d=dma[i];sample=i+1
        if row['caller_pc']!=native['caller_pc']:
            raise ValueError(f'Call-site mismatch at sample {sample}')
        if number(d,'sample')!=sample or d['caller_pc']!=native['caller_pc']:
            raise ValueError('DMA sample/caller identity mismatch')
        em,rm=number(row,'entry_cycle'),number(row,'return_cycle')
        en,rn=number(native,'entry_cycle'),number(native,'return_cycle')
        if (number(d,'entry_cycle'),number(d,'return_cycle'))!=(en,rn):
            raise ValueError('Native DMA boundaries mismatch CPU capture')
        for prefix in ('entry','return'):
            for field in ('a','x','y','sp'):
                if row[prefix+'_'+field]!=native[prefix+'_'+field]:
                    differences.append((sample,prefix+'_'+field))
        cpu_equal=all(row[p+'_'+k]==native[p+'_'+k] for p in ('entry','return')
                      for k in ('a','x','y','sp'))
        cpu_equal &= all(
             (number(row,p+'_p',255)&0xCF)==(number(native,p+'_p',255)&0xCF)
             for p in ('entry','return'))
        ram_equal=row['entry_ram_hex']==native['entry_ram_hex']
        if cpu_equal:aggregate['CPU_registers_and_flags_equal']+=1
        if ram_equal:aggregate['full_entry_RAM_equal']+=1
        if rm-em==rn-en:aggregate['equal_duration']+=1
        md=in_window(mread,em,rm)
        nd=n_by_sample.get(sample,[])
        # Native cycle indices are compared AS RECORDED, not guessed or adjusted.
        ev=compare_events(md,nd)
        if ev['same_exact_read_cycle_and_address']:
            aggregate['DMC_read_cycles_addresses_equal']+=1
        expected=number(d,'dma_cycles')
        actual=all_bus_samples.get(sample,0)
        if expected==actual:aggregate['bus_dma_counts_equal']+=1
        if number(d,'dmc_fetches_confirmed')!=len(nd):
            raise ValueError('Native DMC sidecar conflicts with per-cycle trace')
        pairs.append({
            'sample':sample,'caller_pc':int(row['caller_pc']),
            'entry_cycle_equal':em==en,
            'cpu_registers_flags_equal':bool(cpu_equal),
            'full_entry_ram_equal':ram_equal,
            'duration_mesen':rm-em,'duration_native':rn-en,
            'mesen_real_dmc_reads':len(md),'native_verified_dmc_reads':len(nd),
            'dmc_fetch_cycles_and_addresses_equal':ev['same_exact_read_cycle_and_address'],
            'native_recorded_dma_cycles':expected,
            'native_per_cycle_dma_rows':actual
        })
    result={
      'schema':'cv3dcjp-collision-v12-source-identified-dmc-bus-gate',
      'original_rom_sha256':reference.ROM_SHA,
      'source_sha256':{name:hashlib.sha256(Path(path).read_bytes()).hexdigest()
                      for name,path in [('mesen_cpu',mesen_cpu),
                      ('mesen_dma_bus',mesen_bus),('native_cpu',native_cpu),
                      ('native_dma_bus',native_bus),('native_dma',native_dma)]},
      'paired_ordinal_calls':128,'counts':aggregate,
      'mesen_source_identified_dmc_reads_total':len(mread),
      'native_source_verified_dmc_reads_total':len(nread),
      'full_hardware_gate_passed':False,
      'unverified_requirements':['original engine DMC fetch phase',
           'all bus operations and halt-cycle semantics',
           'IRQ source, NMI/mapper/PPU/APU/VRC6 timing',
           'native C live substitution'],
      'original_6502_PRG_bytes_retired':0,
      'findings':pairs
    }
    out=Path(report_path);out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('rom','mesen-cpu','mesen-bus','native-cpu','native-bus',
              'native-dma','report'):
        p.add_argument('--'+k,required=True)
    a=p.parse_args()
    result=verify(Path(a.rom),a.mesen_cpu,a.mesen_bus,a.native_cpu,
                  a.native_bus,a.native_dma,a.report)
    print('DMC real-read pairs exact: '+str(result['counts']['DMC_read_cycles_addresses_equal'])+
          '/128; full hardware parity: NOT ESTABLISHED')
    return int(result['counts']['DMC_read_cycles_addresses_equal']!=128 or
               result['counts']['CPU_registers_and_flags_equal']!=128)

if __name__=='__main__':
    raise SystemExit(main())
