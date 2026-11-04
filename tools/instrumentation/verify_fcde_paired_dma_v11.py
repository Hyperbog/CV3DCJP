#!/usr/bin/env python3
"""Fail-closed paired FCDE capture and NESRecomp DMA-cycle comparison.

Requires two independent real captures: original Mesen 15-column CSV,
NESRecomp 15-column original-execution CSV, and its traced DMA sidecar.
Same-row entry-state equality is checked before any timing comparison.
A positive result is NOT per-bus-cycle or DMC-emulator parity.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests' / 'references'))
import fcde_reference as reference
import verify_fcde_trace_gate_v11 as gate

DMA_COLUMNS = ('sample', 'caller_pc', 'entry_cycle', 'return_cycle',
               'instruction_cycles', 'dma_cycles', 'dma_halt_cycles',
               'dma_read_cycles', 'dma_write_cycles',
               'dmc_fetches_confirmed', 'oam_dma_writes')
REGS = ('a', 'x', 'y', 'sp')
MAX_SAMPLE = 128


def rows(path, columns):
    with open(path, encoding='utf-8-sig', newline='') as source:
        reader = csv.DictReader(source, strict=True)
        if tuple(reader.fieldnames or ()) != tuple(columns):
            raise ValueError(f'{path}: unexpected capture columns')
        result = list(reader)
    if not 1 <= len(result) <= MAX_SAMPLE:
        raise ValueError(f'{path}: expected 1..{MAX_SAMPLE} records')
    if any(None in row or any(row.get(c) is None for c in columns)
           for row in result):
        raise ValueError(f'{path}: truncated or extra CSV fields')
    return result


def unsigned(row, name, line, ceiling=2**64-1):
    value = row[name]
    if not value or not value.isascii() or not value.isdecimal():
        raise ValueError(f'row {line}: {name} must be unsigned decimal')
    parsed = int(value)
    if parsed > ceiling:
        raise ValueError(f'row {line}: {name} is too large')
    return parsed


def identical_state(a, b, prefix):
    if any(a[prefix+k] != b[prefix+k] for k in REGS):
        return False
    if (int(a[prefix+'p']) & 0xCF) != (int(b[prefix+'p']) & 0xCF):
        return False
    return a[prefix+'ram_hex'] == b[prefix+'ram_hex']


def reference_cycles(rom, row):
    status = int(row['entry_p'])
    output = reference.run_collision(
        rom, bytes.fromhex(row['entry_ram_hex']),
        a=int(row['entry_a']), x=int(row['entry_x']),
        y=int(row['entry_y']), sp=int(row['entry_sp']),
        carry=bool(status&1), zero=bool(status&2),
        negative=bool(status&128), overflow=bool(status&64))
    return output.cycles


def verify(rom_path, mesen_csv, native_csv, native_dma_csv, report_path):
    rom = Path(rom_path).read_bytes()
    # Both images are checked independently. This also enforces the
    # original JSR bytes, return stack, bank selector and 15-column schema.
    gate.parse_rows(mesen_csv, rom)
    gate.parse_rows(native_csv, rom)
    mesen = rows(mesen_csv, gate.FIELDS)
    native = rows(native_csv, gate.FIELDS)
    dma = rows(native_dma_csv, DMA_COLUMNS)
    if len(mesen) != len(native) or len(mesen) != len(dma):
        raise ValueError('capture lengths differ: cannot pair samples')
    failures, results = [], []
    for i, (m, n, d) in enumerate(zip(mesen, native, dma), 1):
        errors = []
        caller = unsigned(m, 'caller_pc', i, 65535)
        if caller != unsigned(n, 'caller_pc', i, 65535):
            errors.append('caller_order')
        if not identical_state(m, n, 'entry_'):
            errors.append('entry_state_not_synchronized')
        if not identical_state(m, n, 'return_'):
            errors.append('return_state')
        if unsigned(d, 'sample', i, MAX_SAMPLE) != i:
            errors.append('dma_sequence')
        if unsigned(d, 'caller_pc', i, 65535) != caller:
            errors.append('dma_caller')
        for key in ('entry_cycle', 'return_cycle'):
            if unsigned(d, key, i) != unsigned(n, key, i):
                errors.append('dma_'+key)
        try:
            rom_clocks = reference_cycles(rom, m)
        except (ValueError, IndexError) as exc:
            errors.append('rom_reference:'+str(exc))
            rom_clocks = None
        mesen_clocks = unsigned(m,'return_cycle',i) - unsigned(m,'entry_cycle',i)
        native_clocks = unsigned(n,'return_cycle',i) - unsigned(n,'entry_cycle',i)
        stalls = unsigned(d,'dma_cycles',i)
        halts = unsigned(d,'dma_halt_cycles',i)
        reads = unsigned(d,'dma_read_cycles',i)
        writes = unsigned(d,'dma_write_cycles',i)
        dmc = unsigned(d,'dmc_fetches_confirmed',i)
        oam = unsigned(d,'oam_dma_writes',i)
        if stalls != halts+reads+writes:
            errors.append('dma_cycle_accounting')
        if dmc > reads or oam > writes:
            errors.append('dma_device_counts')
        if rom_clocks is not None:
            if unsigned(d,'instruction_cycles',i) != rom_clocks:
                errors.append('native_shadow_rom_instruction_cycles')
            if native_clocks != rom_clocks + stalls:
                errors.append('native_dma_time_not_accounted')
            if mesen_clocks != native_clocks:
                errors.append('cross_emulator_total_cycles')
        else:
            errors.append('rom_instruction_unavailable')
        item = {'sample':i, 'caller':f'0x{caller:04X}',
                'mesen_cycles':mesen_clocks,
                'native_cycles':native_clocks,
                'rom_instruction_cycles':rom_clocks,
                'nesrecomp_dma_cycles':stalls,
                'nesrecomp_dmc_fetches_confirmed':dmc,
                'mesen_clock_residual':None if rom_clocks is None
                                         else mesen_clocks-rom_clocks,
                'equal':not errors, 'differences':errors}
        results.append(item)
        if errors: failures.append(item)
    report = {
        'schema':'cv3dcjp-fcde-paired-dma-v11',
        'rom_sha256':reference.ROM_SHA,
        'mesen_capture_sha256':hashlib.sha256(Path(mesen_csv).read_bytes()).hexdigest(),
        'native_capture_sha256':hashlib.sha256(Path(native_csv).read_bytes()).hexdigest(),
        'native_dma_sha256':hashlib.sha256(Path(native_dma_csv).read_bytes()).hexdigest(),
        'samples':len(results), 'pairs_matching':len(results)-len(failures),
        'pairs_failing':len(failures),
        'synchronized_start_and_movie_provenance_verified':False,
        'native_execution_performed_by_this_script':False,
        'mesen_dmc_bus_events_available':False,
        'dmc_dma_causality_verified':False,
        'cpu_bus_access_parity_verified':False,
        'native_substitution_authorized':False,
        'failures':failures,
    }
    out=Path(report_path)
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('rom','mesen','native','dma','report'):
        parser.add_argument('--'+name,required=True)
    args=parser.parse_args()
    r=verify(args.rom,args.mesen,args.native,args.dma,args.report)
    print(f"Paired FCDE: {r['pairs_matching']}/{r['samples']} "
          "entry/return/timing/DMA checks; no bus or DMC parity claim")
    return int(bool(r['pairs_failing']))


if __name__=='__main__':
    raise SystemExit(main())
