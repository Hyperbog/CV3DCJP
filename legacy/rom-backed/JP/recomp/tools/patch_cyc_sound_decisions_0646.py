#!/usr/bin/env python3
"""JP0646: add original sound-driver decision taps to JP0612 private overlay.

Patches only the already-generated mapper24 JP0612 hw_machine.c overlay.
Valid opcode fetches go through the same interpreter/JIT dual read paths;
no original hardware or CPU execution code is modified in the repo.
"""
from __future__ import annotations

import argparse
from pathlib import Path

MARKER="CVIII_SOUND_DECISIONS_0646"
INCLUDE='#include "cviii_sound_requests_0612.h"'
OLD='''    if (!cpu_opcode_fetch || addr != 0xE249u || opcode != 0x48u ||
        hw_cart.mapper != 24 || cpu.do_nmi || cpu.do_irq || cpu.do_reset)
        return;
    const uint8_t lo=hw.ram[0x100u + (uint8_t)(cpu.s+1u)];'''
NEW='''    /* CVIII_SOUND_DECISIONS_0646: read-only original bank0C dispatch,
     * after an actual opcode fetch; do not count interrupted fetches.
     * The original PRG wrapper's bank markers are context evidence,
     * not themselves a substitute for runtime Mesen parity. */
    if (!cpu_opcode_fetch || hw_cart.mapper != 24 ||
        cpu.do_nmi || cpu.do_irq || cpu.do_reset)
        return;
    if (hw.ram[0x23u]==0x0Cu && hw.ram[0x26u]==0xFFu)
        cviii_sound_decisions_fetch(hw.cycles,addr,opcode,hw.ram);
    if (addr != 0xE249u || opcode != 0x48u)return;
    const uint8_t lo=hw.ram[0x100u + (uint8_t)(cpu.s+1u)];'''


def once(s: str, old: str, new: str) -> str:
    if s.count(old) != 1:
        raise ValueError("JP0646 pinned hardware source anchor absent/ambiguous")
    return s.replace(old,new,1)


def generate(source: str) -> str:
    if MARKER in source or source.count("CVIII_SOUND_REQUEST_0612") != 1:
        raise ValueError("JP0646 requires unmodified generated JP0612 hardware")
    source=once(source,INCLUDE,INCLUDE+'\n#include "cviii_sound_decisions_0646.h"')
    source=once(source,OLD,NEW)
    if source.count("cviii_observe_sound_fetch(addr,value)")!=2:
        raise ValueError("JP0646 lost one of native interpreter/JIT fetch taps")
    return source


def smoke() -> None:
    from patch_cyc_sound_requests_0612 import patch, smoke as prior_smoke
    prior_smoke()
    upstream='''#include "hw_internal.h"
uint8_t hw_read(uint16_t addr)
{
    return hw_bus_read(addr);
}
    if (cyc_trace_enabled) cyc_trace_access(addr, value, false);
    return value;
}'''
    base=patch(upstream)
    output=generate(base)
    assert output.count(MARKER)==1
    assert "hw.ram[0x23u]==0x0Cu && hw.ram[0x26u]==0xFFu" in output
    assert "cpu.do_nmi || cpu.do_irq || cpu.do_reset" in output
    assert "cviii_sound_requests_observe(" in output
    assert output.count("cviii_observe_sound_fetch(addr,value)")==2
    for corrupt in (output, base.replace("opcode != 0x48u", "opcode != 0x49u"),
                    base.replace(INCLUDE,"// missing header")):
        try: generate(corrupt)
        except ValueError: continue
        raise AssertionError("JP0646 accepted altered double-patched source")
    print("PASS JP0646 read-only dual-fetch original driver decision tap")


def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--smoke",action="store_true")
    ap.add_argument("--source",type=Path)
    ap.add_argument("--out",type=Path)
    args=ap.parse_args()
    if args.smoke:smoke();return
    if not args.source or not args.out:
        ap.error("provide --source and --out")
    source=args.source.resolve(strict=True)
    out=args.out.resolve()
    if source.parent!=out.parent or source==out or \
       source.name!="cviii_hw_machine_requests.c" or \
       out.name!="cviii_hw_machine_decisions.c":
        raise ValueError("JP0646 must patch adjacent JP0612 generated private hardware")
    out.write_text(generate(source.read_text(encoding="utf-8")),encoding="utf-8")
    print("PASS JP0646 private read-only native sound-driver opcode hook")


if __name__=="__main__":
    main()
