#!/usr/bin/env python3
"""JP0600: private, read-only VRC6 PPU CHR fetch instrumentation overlay.

The external pinned runtime is never edited. CMake writes another source in
its private checkout with a hook that is inert unless the Enhanced host
explicitly enables one sampled audit frame. No copyrighted ROM is included.
"""
from __future__ import annotations

import argparse
from pathlib import Path

MARKER = "CVIII_CHR_FETCH_OBSERVER_0600"
ORIGINAL = """    uint8_t value = hw_cart.chr[hw_cart_chr_index(addr)];
    if ((hw_cart.mapper == 9 || hw_cart.mapper == 10) && !hw_cart.m.pattern_pending) {"""
REPLACEMENT = """    uint32_t physical = hw_cart_chr_index(addr);
    uint8_t value = hw_cart.chr[physical];
    /* CVIII_CHR_FETCH_OBSERVER_0600: no mutation, no PPU byte substitution.
     * Sampled only when the owner-private Enhanced host enables its hook.
     * The caller has already driven the actual PPU /RD pin in hw_ppu.c. */
    if (cviii_chr_fetch_observer && hw_cart.mapper == 24)
        cviii_chr_fetch_observer(addr, physical, value, ppu.scanline, ppu.dot);
    if ((hw_cart.mapper == 9 || hw_cart.mapper == 10) && !hw_cart.m.pattern_pending) {"""
HOOK = """/* Game-owned read-only diagnostic hook; NULL is always the stock path. */
static CviiiChrFetchHook cviii_chr_fetch_observer = NULL;
void cviii_chr_fetch_set_hook(CviiiChrFetchHook observer)
{
    cviii_chr_fetch_observer = observer;
}
"""


def once(text: str, needle: str, replacement: str) -> str:
    if text.count(needle) != 1:
        raise ValueError("Pinned upstream mapper source changed: " + needle[:90])
    return text.replace(needle, replacement, 1)


def generate(source: str) -> str:
    if MARKER in source:
        raise ValueError("JP0600 mapper overlay already installed")
    source = once(source, '#include "hw_internal.h"',
                  '#include "hw_internal.h"\n#include "cviii_chr_fetch_0600.h"')
    source = once(source, 'uint8_t hw_cart_chr_read(uint16_t addr)',
                  HOOK + '\nuint8_t hw_cart_chr_read(uint16_t addr)')
    source = once(source, ORIGINAL, REPLACEMENT)
    return source


def smoke() -> None:
    fixture = ('#include "hw_internal.h"\n'
               'uint8_t hw_cart_chr_read(uint16_t addr) {\n'
               + ORIGINAL + '\n}\n')
    result = generate(fixture)
    assert result.count(MARKER) == 1
    assert "hw_cart.mapper == 24" in result
    assert "cviii_chr_fetch_observer(addr, physical, value, ppu.scanline, ppu.dot)" in result
    assert result.count('if ((hw_cart.mapper == 9 || hw_cart.mapper == 10)') == 1
    try:
        generate(result)
    except ValueError:
        pass
    else:
        raise AssertionError("Duplicate private mapper patch was accepted")
    print("PASS JP0600 private CHR read-only tap patch/anchor contract")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--smoke", action="store_true")
    a = parser.parse_args()
    if a.smoke:
        smoke()
        return
    if not a.source or not a.out:
        parser.error("--source and --out required")
    src = a.source.resolve(strict=True)
    out = a.out.resolve()
    if (src.name != "hw_mapper.c" or out.name != "cviii_hw_mapper_fetch.c"
        or out.parent != src.parent):
        raise ValueError("Must generate adjacent private hw_mapper.c overlay")
    out.write_text(generate(src.read_text(encoding="utf-8")), encoding="utf-8")
    print("PASS JP0600 read-only private VRC6 CHR fetch observer: " + str(out))


if __name__ == "__main__":
    main()
