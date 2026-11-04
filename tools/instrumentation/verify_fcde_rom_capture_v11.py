#!/usr/bin/env python3
"""Replay captured Mesen FCDE entries through original Japanese ROM opcodes.

Checks 6502 instruction totals and CPU state only. Does not prove DMC DMA,
live native-C substitution, emulator provenance, or bus-cycle timing.
No original ROM, RAM snapshot, or full private capture is written to the report.
"""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests' / 'references'))
sys.path.insert(0, str(ROOT / 'tools' / 'instrumentation'))
import fcde_reference as reference
import verify_fcde_trace_gate_v11 as gate


def verify(rom_path, trace_path, out_path):
    rom = Path(rom_path).read_bytes()
    # This rejects non-matching ROMs, unverified JSR sites, malformed stack,
    # incorrect selector RAM, and impossible or malformed CSV rows.
    gate.parse_rows(trace_path, rom)
    totals = Counter()
    callers = Counter()
    residuals = Counter()
    failures = []
    with open(trace_path, newline='', encoding='utf-8-sig') as file:
        reader = csv.DictReader(file)
        for line, row in enumerate(reader, 2):
            initial_p = int(row['entry_p'])
            outcome = reference.run_collision(
                rom,
                bytes.fromhex(row['entry_ram_hex']),
                a=int(row['entry_a']), x=int(row['entry_x']),
                y=int(row['entry_y']), sp=int(row['entry_sp']),
                carry=bool(initial_p & 1), zero=bool(initial_p & 2),
                negative=bool(initial_p & 0x80),
                overflow=bool(initial_p & 0x40))
            caller = int(row['caller_pc'])
            cycles = int(row['return_cycle']) - int(row['entry_cycle'])
            diffs = []
            for k, actual in [('a',outcome.a),('x',outcome.x),
                              ('y',outcome.y),('sp',(outcome.sp+2)&255)]:
                if actual != int(row['return_'+k]): diffs.append(k)
            flags = ((initial_p & 0x3c) | int(outcome.carry) |
                     (int(outcome.zero)<<1) | (int(outcome.overflow)<<6) |
                     (int(outcome.negative)<<7))
            if (flags&0xcf) != (int(row['return_p'])&0xcf):
                diffs.append('p')
            returned = bytearray.fromhex(row['entry_ram_hex'])
            for addr in outcome.ram_delta:
                returned[int(addr,16)] = outcome.ram_delta[addr]['new']
            if returned != bytearray.fromhex(row['return_ram_hex']):
                diffs.append('ram_2k')
            if cycles != outcome.cycles: diffs.append('cpu_cycles')
            totals['samples'] += 1
            totals['exact_matches' if not diffs else 'mismatches'] += 1
            callers[f'${caller:04X}'] += 1
            residuals[str(cycles-outcome.cycles)] += 1
            for name in diffs: totals['field_diff_'+name] += 1
            if diffs:
                failures.append({'line':line, 'caller':f'${caller:04X}',
                                 'fields':diffs,
                                 'observed_cycles':cycles,
                                 'instruction_cycles':outcome.cycles})
    report = {
        'schema':'cv3dcjp-jp-fcde-live-capture-rom-opcode-replay-v11',
        'rom_sha256':reference.ROM_SHA,
        'capture_kind':'user-supplied Mesen-labeled CSV, provenance unverified',
        'samples':totals['samples'],
        'exact_matches':totals['exact_matches'],
        'mismatches':totals['mismatches'],
        'callers':dict(callers),
        'observed_minus_rom_instruction_cycles':dict(residuals),
        'field_difference_counts':{k.removeprefix('field_diff_'):v
                                   for k,v in totals.items()
                                   if k.startswith('field_diff_')},
        'differences':failures,
        'rom_opcode_reference_executed':True,
        'native_c_compared':False,
        'dmc_dma_observed':False,
        'hardware_bus_cycle_parity_verified':False,
        'native_runtime_substitution_executed':False
    }
    Path(out_path).parent.mkdir(parents=True,exist_ok=True)
    Path(out_path).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',required=True)
    p.add_argument('--trace',required=True)
    p.add_argument('--report',required=True)
    a=p.parse_args()
    result=verify(a.rom,a.trace,a.report)
    print(f"FCDE original ROM replay: {result['exact_matches']}/"
          f"{result['samples']} exact; "
          f"clock residuals={result['observed_minus_rom_instruction_cycles']}")
    return int(bool(result['mismatches']))


if __name__=='__main__':
    raise SystemExit(main())
