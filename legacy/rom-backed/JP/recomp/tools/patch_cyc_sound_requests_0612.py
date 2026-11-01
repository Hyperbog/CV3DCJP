#!/usr/bin/env python3
"""Create private hw_machine.c overlay observing *executed* JP E249 fetches.

Both recompiled hw_read_rom and interpreter hw_read are covered; an
interrupt-discarded opcode fetch does NOT count as a request. Fail closed
if upstream contracts change. Never modify upstream or game ROM in place.
"""
from __future__ import annotations
import argparse
from pathlib import Path

MARK = 'CVIII_SOUND_REQUEST_0612'
INCLUDE = '#include "hw_internal.h"'
READ = '''uint8_t hw_read(uint16_t addr)
{
    return hw_bus_read(addr);
}'''
READ_ROM = '''    if (cyc_trace_enabled) cyc_trace_access(addr, value, false);
    return value;
}'''
HOOK = '''/* CVIII_SOUND_REQUEST_0612: no 6502 or device side effects.
 * Called after opcode bytes are read, once per real fetch. Ignore a fetch
 * that will be discarded for an NMI, IRQ or RESET, and non-JP mappers.
 * The two stack bytes are not necessarily a JSR return address. */
static void cviii_observe_sound_fetch(uint16_t addr, uint8_t opcode)
{
    if (!cpu_opcode_fetch || addr != 0xE249u || opcode != 0x48u ||
        hw_cart.mapper != 24 || cpu.do_nmi || cpu.do_irq || cpu.do_reset)
        return;
    const uint8_t lo=hw.ram[0x100u + (uint8_t)(cpu.s+1u)];
    const uint8_t hi=hw.ram[0x100u + (uint8_t)(cpu.s+2u)];
    cviii_sound_requests_observe(hw.cycles,addr,cpu.a,cpu.x,cpu.y,
        cpu.s,cpu_get_p(0),hw.ram[0x23],lo,hi);
}
'''
REVISED_READ = '''uint8_t hw_read(uint16_t addr)
{
    uint8_t value=hw_bus_read(addr);
    cviii_observe_sound_fetch(addr,value);
    return value;
}'''
REVISED_ROM = '''    if (cyc_trace_enabled) cyc_trace_access(addr, value, false);
    cviii_observe_sound_fetch(addr,value);
    return value;
}'''


def once(source: str, old: str, replacement: str) -> str:
    if source.count(old) != 1:
        raise ValueError('Unexpected pinned hardware source: '+old[:45])
    return source.replace(old,replacement,1)


def patch(source: str) -> str:
    if MARK in source:
        raise ValueError('Duplicate JP0612 hardware fetch observer')
    source=once(source,INCLUDE,INCLUDE+'\n#include "cpu6502.h"\n'
                '#include "cviii_sound_requests_0612.h"')
    source=once(source,READ,HOOK+'\n'+REVISED_READ)
    source=once(source,READ_ROM,REVISED_ROM)
    return source


def smoke() -> None:
    src=INCLUDE+'\nstatic bool cpu_opcode_fetch;\n'+READ+'\n'+\
        'uint8_t hw_read_rom(uint16_t addr, uint8_t value) {\n'+READ_ROM+'\n'
    result=patch(src)
    assert result.count('cviii_observe_sound_fetch(addr,value)') == 2
    assert 'cpu.do_nmi || cpu.do_irq || cpu.do_reset' in result
    assert 'addr != 0xE249u || opcode != 0x48u' in result
    assert 'hw_cart.mapper != 24' in result
    assert 'uint8_t value=hw_bus_read(addr);' in result
    try: patch(result)
    except ValueError: pass
    else: raise AssertionError('Duplicate JP0612 tap accepted')
    for change in (src.replace('return hw_bus_read(addr);','return 0;'),
                   src.replace('cyc_trace_access(addr, value, false);','')):
        try: patch(change)
        except ValueError: continue
        raise AssertionError('Changed upstream anchor accepted')
    print('PASS JP0612 dual-fetch patch: 2 sites, interrupt suppression, fail closed')


def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--smoke',action='store_true')
    ap.add_argument('--source',type=Path)
    ap.add_argument('--out',type=Path)
    args=ap.parse_args()
    if args.smoke: smoke(); return
    if not args.source or not args.out: ap.error('--source and --out required')
    source=args.source.resolve(strict=True); out=args.out.resolve()
    if source.name!='hw_machine.c' or out.name!='cviii_hw_machine_requests.c' or source.parent!=out.parent:
        raise ValueError('Expected private adjacent pinned hw_machine.c overlay')
    out.write_text(patch(source.read_text(encoding='utf-8')),encoding='utf-8')
    print('JP0612 sound-request fetch observer generated in private runtime')

if __name__=='__main__': main()
