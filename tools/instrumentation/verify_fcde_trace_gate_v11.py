#!/usr/bin/env python3
"""Strict offline preflight for v10 Mesen2 FCDE CSV; never authorizes substitution.

ROM/static byte checks are independent from measured CPU-clock comparisons.
CSV contents and its SHA256 are NOT cryptographic proof of emulator provenance.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path

import verify_fcde_live_trace_v10 as baseline

ROM_HASH = baseline.SHA
MAX_ROWS = 128  # v10 Lua cap, per capture file
FIELDS = ('caller_pc', 'entry_cycle', 'return_cycle',
          'entry_a', 'entry_x', 'entry_y', 'entry_sp', 'entry_p',
          'return_a', 'return_x', 'return_y', 'return_sp', 'return_p',
          'entry_ram_hex', 'return_ram_hex')
# Exact v10 instrumented sites; verified against authentic bank-14 ROM bytes.
SITES = (0x8345, 0x834e, 0x839d, 0x83ea, 0x83f6, 0x8429,
         0x8450, 0x84cf, 0x84d8, 0x9153, 0x9164, 0x91cf,
         0x91d8, 0x9212, 0x96cf, 0x96d8, 0x96e4, 0x96ed,
         0x970b, 0x9714, 0x9720, 0x9729, 0x97b1, 0x97ba,
         0x97c5, 0x97ce, 0x97e7, 0x97fc, 0x9b3c, 0x9b45,
         0x9b4e, 0x9b59, 0x9b7b, 0x9b84, 0x9b8d, 0x9b96)


def decimal(value, field, line, maximum):
    if not value or not value.isascii() or not value.isdecimal():
        raise ValueError(f'line {line}: {field} must be unsigned decimal')
    number = int(value)
    if number > maximum:
        raise ValueError(f'line {line}: {field} is out of range')
    return number


def parse_rows(path, rom):
    """Reject corrupt/mispaired captures BEFORE invoking any native code."""
    if len(rom) != 393232 or hashlib.sha256(rom).hexdigest() != ROM_HASH:
        raise ValueError('Original Japanese ROM size/SHA256 mismatch')
    for pc in SITES:
        addr = 0x38010 + (pc - 0x8000)
        if rom[addr:addr + 3] != b'\x20\xde\xfc':
            raise ValueError(f'authenticated site ${pc:04X} no longer JSR $FCDE')
    rows = []
    with open(path, newline='', encoding='utf-8-sig') as file:
        reader = csv.DictReader(file, strict=True)
        if reader.fieldnames != list(FIELDS):
            raise ValueError('trace header is not the exact v10 schema')
        for line, row in enumerate(reader, 2):
            if len(rows) >= MAX_ROWS:
                raise ValueError('more rows than a single v10 capture can produce')
            if None in row or any(row[key] is None for key in FIELDS):
                raise ValueError(f'line {line}: missing/extra CSV fields')
            pc = decimal(row['caller_pc'], 'caller_pc', line, 65535)
            if pc not in SITES:
                raise ValueError(f'line {line}: caller ${pc:04X} was not instrumented')
            start = decimal(row['entry_cycle'], 'entry_cycle', line, 2**64 - 1)
            end = decimal(row['return_cycle'], 'return_cycle', line, 2**64 - 1)
            if end <= start:
                raise ValueError(f'line {line}: nonpositive cycle delta')
            for prefix in ('entry_', 'return_'):
                for field in ('a', 'x', 'y', 'sp', 'p'):
                    decimal(row[prefix + field], prefix + field, line, 255)
                raw = row[prefix + 'ram_hex']
                if len(raw) != 4096 or any(c not in '0123456789abcdef' for c in raw):
                    raise ValueError(f'line {line}: RAM must be 2048 lowercase hex bytes')
            entry_ram = bytes.fromhex(row['entry_ram_hex'])
            sp = int(row['entry_sp'])
            ret = entry_ram[0x100 + ((sp + 1) & 255)]
            ret |= entry_ram[0x100 + ((sp + 2) & 255)] << 8
            if ret != pc + 2:
                raise ValueError(f'line {line}: original JSR stack return mismatch')
            if entry_ram[0x23] != 14:
                raise ValueError(f'line {line}: bank-14 selector not present')
            rows.append({'line': line, 'caller_pc': f'${pc:04X}',
                         'entry_cycle': start, 'return_cycle': end})
    if not rows:
        raise ValueError('empty trace cannot establish CPU parity')
    return rows


def run(rom_path, trace_path, report_path):
    rom_path, trace_path, report_path = map(Path, (rom_path, trace_path, report_path))
    rom = rom_path.read_bytes()
    rows = parse_rows(trace_path, rom)
    # The v10 verifier compiles the independent native C routine and compares
    # all captured A/X/Y/SP/P, 2-KiB RAM, and total CPU-cycle counts.
    result = baseline.verify(rom_path, trace_path, report_path)
    observed = sorted({row['caller_pc'] for row in rows})
    result.update({
        'schema': 'cv3dcjp-fcde-offline-preflight-v11',
        'capture_file_sha256': hashlib.sha256(trace_path.read_bytes()).hexdigest(),
        'capture_provenance_verified': False,
        'capture_configured_rom_sites': len(SITES),
        'capture_observed_rom_sites': len(observed),
        'observed_sites': observed,
        'row_preflight_passed': True,
        'live_substitution_executed': False,
        'bus_cycle_parity_verified': False,
        'native_runtime_commit_authorized': False,
        'blocking_gates': ['real Mesen2 capture provenance',
                           'per-CPU-cycle bus/PPU/APU/DMA/VRC6/IRQ/NMI matching',
                           'validated NESRecomp instruction-fetch interception'],
    })
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True)
    parser.add_argument('--trace', required=True)
    parser.add_argument('--report', required=True)
    args = parser.parse_args()
    result = run(args.rom, args.trace, args.report)
    print(f"FCDE v11 preflight: {result['matches']}/{result['samples']} offline matches, "
          f"{result['capture_observed_rom_sites']}/{len(SITES)} instrumented sites; "
          '0 live substitutions')
    return int(bool(result['mismatches']))


if __name__ == '__main__':
    raise SystemExit(main())
