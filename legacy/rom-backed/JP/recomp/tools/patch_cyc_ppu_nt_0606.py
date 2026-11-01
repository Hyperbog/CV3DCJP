#!/usr/bin/env python3
"""JP0606 read-only PPU nametable/attribute fetch overlay for pinned NESRecomp.

The original hw_ppu.c is unchanged. This generated source is private inside
the owner's external runtime checkout; it does not alter PPU return values.
"""
from __future__ import annotations

import argparse
from pathlib import Path

MARK = "CVIII_PPU_NT_HOOK_0606"
READ = "        if (!hw_cart_nt_read(addr, ppu.rd != 0, &value)) value = ppu.ciram[ciram_index()];"
INSERT = READ + """
        /* CVIII_PPU_NT_HOOK_0606: only real /RD-driven nametable
         * and attribute bytes, after the stock cartridge/CIRAM read.
         * The observed byte continues unchanged to the PPU bus. */
        if (cviii_nt_callback && hw_cart.mapper == 24 && ppu.rd)
            cviii_nt_callback(addr, value,
                (uint8_t)((hw_cart.m.ctrl & 0x10u) != 0),
                (uint16_t)ppu.scanline, (uint16_t)ppu.dot);"""
HOOK = """static CviiiNtHook cviii_nt_callback;
void cviii_nt_set_hook(CviiiNtHook callback)
{
    cviii_nt_callback = callback;
}
"""


def one(source: str, old: str, new: str) -> str:
    if source.count(old) != 1:
        raise ValueError("Pinned NESRecomp PPU source contract changed: " + old[:90])
    return source.replace(old, new, 1)


def patch(source: str) -> str:
    if MARK in source:
        raise ValueError("JP0606 diagnostic was already added")
    source = one(source, '#include "hw_internal.h"',
                 '#include "hw_internal.h"\n#include "cviii_nt_trace_0606.h"')
    source = one(source, "HwPpu    ppu;", "HwPpu    ppu;\n" + HOOK)
    return one(source, READ, INSERT)


def smoke() -> None:
    tiny = ('#include "hw_internal.h"\n'
            'HwPpu    ppu;\n'
            'static uint8_t vram_fetch(void) {\n'
            + READ + '\n}\n')
    result = patch(tiny)
    assert result.count(MARK) == 1
    assert result.count(READ) == 1
    assert "hw_cart.mapper == 24 && ppu.rd" in result
    assert "(uint8_t)((hw_cart.m.ctrl & 0x10u) != 0)" in result
    try:
        patch(result)
    except ValueError:
        pass
    else:
        raise AssertionError("Repeated source overlay accepted")
    print("PASS JP0606 exact pinned PPU read-only nametable overlay contract")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", type=Path)
    p.add_argument("--out", type=Path)
    p.add_argument("--smoke", action="store_true")
    args = p.parse_args()
    if args.smoke:
        smoke()
        return
    if not args.source or not args.out:
        p.error("--source and --out required")
    source = args.source.resolve(strict=True)
    output = args.out.resolve()
    if (source.name != "hw_ppu.c" or output.name != "cviii_hw_ppu_nt.c"
            or source.parent != output.parent):
        raise ValueError("Expected private adjacent pinned PPU overlay")
    output.write_text(patch(source.read_text(encoding="utf-8")), encoding="utf-8")
    print(f"JP0606 private read-only PPU nametable overlay: {output}")


if __name__ == "__main__":
    main()
