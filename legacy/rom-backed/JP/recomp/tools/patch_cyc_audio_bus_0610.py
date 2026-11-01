#!/usr/bin/env python3
"""JP0610: generate owner-private APU/VRC6 register write observers.

Only taps real CPU writes after their host functions are reached.
Original register implementation executes unchanged (same order/value).
No licensed upstream runtime or original ROM is committed.
"""
from __future__ import annotations
import argparse
from pathlib import Path

APU_SIGNATURE = """void apu_write(uint16_t addr, uint8_t value)
{
    if (addr <= 0x400F) apu.reg[addr & 15] = value;"""
APU_NEW = """void apu_write(uint16_t addr, uint8_t value)
{
    /* JP0610 observe audio registers only: not $4014 DMA or $4016 input. */
    if ((addr >= 0x4000u && addr <= 0x4013u) ||
        addr == 0x4015u || addr == 0x4017u)
        cviii_audio_bus_observe(hw.cycles, addr, value);
    if (addr <= 0x400F) apu.reg[addr & 15] = value;"""
MAPPER_SIGNATURE = """void hw_cart_cpu_write(uint16_t addr, uint8_t value)
{
    if (hw_cart.mapper==20)"""
MAPPER_NEW = """void hw_cart_cpu_write(uint16_t addr, uint8_t value)
{
    /* JP0610: VRC6's three audio voices plus $9003 global control;
     * mapper24 only; no CHR/PRG writes or altered side effects. */
    if (hw_cart.mapper == 24 &&
        (((addr & 0xf000u) == 0x9000u && (addr & 3u) <= 3u) ||
         (((addr & 0xf000u) == 0xa000u ||
           (addr & 0xf000u) == 0xb000u) && (addr & 3u) <= 2u)))
        cviii_audio_bus_observe(hw.cycles, addr, value);
    if (hw_cart.mapper==20)"""
INCLUDE = '#include "hw_internal.h"'
MARK = 'JP0610'


def replace_exact(s: str, anchor: str, replacement: str) -> str:
    if s.count(anchor) != 1:
        raise ValueError("Pinned upstream contract changed: " + anchor[:75])
    return s.replace(anchor, replacement, 1)


def patch(s: str, kind: str) -> str:
    if MARK in s:
        raise ValueError("Refusing duplicate JP0610 host observer")
    s = replace_exact(s, INCLUDE, INCLUDE + '\n#include "cviii_audio_bus_0610.h"')
    if kind == "apu":
        return replace_exact(s, APU_SIGNATURE, APU_NEW)
    if kind == "mapper":
        return replace_exact(s, MAPPER_SIGNATURE, MAPPER_NEW)
    raise ValueError("Invalid patch kind")


def smoke() -> None:
    for kind, anchor in (("apu", APU_SIGNATURE), ("mapper", MAPPER_SIGNATURE)):
        x = INCLUDE + "\n" + anchor + "\n"
        out = patch(x, kind)
        assert out.count("cviii_audio_bus_observe") == 1
        assert anchor not in out
        try:
            patch(out, kind)
        except ValueError:
            pass
        else:
            raise AssertionError("Duplicate observer patch permitted")
    print("PASS JP0610 actual CPU APU/VRC6 register taps, no write diversion")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--smoke", action="store_true")
    p.add_argument("--apu", type=Path)
    p.add_argument("--mapper", type=Path)
    p.add_argument("--out-apu", type=Path)
    p.add_argument("--out-mapper", type=Path)
    opts = p.parse_args()
    if opts.smoke:
        smoke()
        return
    if any(x is None for x in (opts.apu, opts.mapper, opts.out_apu, opts.out_mapper)):
        p.error("--apu, --mapper, --out-apu and --out-mapper required")
    a, m = opts.apu.resolve(strict=True), opts.mapper.resolve(strict=True)
    ao, mo = opts.out_apu.resolve(), opts.out_mapper.resolve()
    if (a.name != "hw_apu.c" or m.name != "cviii_hw_mapper_fetch.c" or
            ao.name != "cviii_hw_apu_audio.c" or
            mo.name != "cviii_hw_mapper_fetch_audio.c" or
            a.parent != ao.parent or m.parent != mo.parent):
        raise ValueError("Expected sibling files of the pinned private NESRecomp host")
    ao.write_text(patch(a.read_text(encoding="utf-8"), "apu"), encoding="utf-8")
    mo.write_text(patch(m.read_text(encoding="utf-8"), "mapper"), encoding="utf-8")
    print("JP0610 private APU/VRC6 register observer overlays generated")


if __name__ == "__main__":
    main()
