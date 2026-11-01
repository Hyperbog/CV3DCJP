#!/usr/bin/env python3
"""JP0650: patch actual pinned JP0610 APU+VRC6 overlays for real DMA evidence.

The APU callback is in dmc_dma_get immediately AFTER one actual bus read,
using the mapper physical offset captured BEFORE that read. The mapper
callback is BEFORE a mapper24 $C000..CFFF bank-write dispatch and is marked
as an intent; true bank selection is proven only by the DMA physical offset.
Both upstream files stay untouched in private external NESRecomp checkout.
"""
from __future__ import annotations
import argparse
from pathlib import Path

INCLUDE='#include "cviii_audio_bus_0610.h"'
TAG='CVIII_DMC_DMA_TRACE_0650'
APU_OLD='''static void dmc_dma_get(void)
{
    apu.dmc_buffer = hw_bus_read(apu.dmc_addr);'''
APU_NEW='''static void dmc_dma_get(void)
{
    /* CVIII_DMC_DMA_TRACE_0650: true DMC get, not halt/dummy CPU read.
     * Save physical PRG mapping BEFORE hw_bus_read and log its returned
     * data AFTER the existing bus side effects; CPU/APU state unchanged. */
    const uint16_t observed_addr=apu.dmc_addr;
    const uint32_t observed_phys=hw_cart.prg_off[(observed_addr>>12)&7u] |
                                 (observed_addr&0x0FFFu);
    apu.dmc_buffer = hw_bus_read(apu.dmc_addr);
    if(hw_cart.mapper==24)
        cviii_dmc_dma_trace_fetch(hw.cycles,observed_addr,observed_phys,
                                 apu.dmc_buffer,apu.dmc_bytes);'''
MAPPER_OLD='''void hw_cart_cpu_write(uint16_t addr, uint8_t value)
{
    /* JP0610: VRC6's three audio voices plus $9003 global control;'''
MAPPER_NEW='''void hw_cart_cpu_write(uint16_t addr, uint8_t value)
{
    /* CVIII_DMC_DMA_TRACE_0650: mapper24 PRG8 *write intent* only;
     * the original downstream mapper operation still owns the mapping. */
    if(hw_cart.mapper==24 && (addr&0xF000u)==0xC000u)
        cviii_dmc_dma_trace_bank_write(hw.cycles,addr,value);
    /* JP0610: VRC6's three audio voices plus $9003 global control;'''

def once(source:str,old:str,new:str)->str:
    if source.count(old)!=1:
        raise ValueError("JP0650 pinned runtime APU/mapper anchor missing or repeated")
    return source.replace(old,new,1)

def patch(source:str,kind:str)->str:
    if TAG in source or source.count(INCLUDE)!=1:
        raise ValueError("JP0650 requires exactly one original JP0610 audio overlay")
    source=once(source,INCLUDE,INCLUDE+'\n#include "cviii_dmc_dma_trace_0650.h"')
    if kind=='apu':
        if source.count('cviii_audio_bus_observe(hw.cycles, addr, value)')!=1:
            raise ValueError("JP0650 lost earlier 2A03 observer")
        source=once(source,APU_OLD,APU_NEW)
        if source.count('apu.dmc_buffer = hw_bus_read(apu.dmc_addr);')!=1:
            raise ValueError("JP0650 duplicated a DMA hardware read")
    elif kind=='mapper':
        if source.count('cviii_audio_bus_observe(hw.cycles, addr, value)')!=1:
            raise ValueError("JP0650 lost earlier VRC6 audio observer")
        source=once(source,MAPPER_OLD,MAPPER_NEW)
    else:raise ValueError("invalid JP0650 external source")
    return source

def smoke()->None:
    from patch_cyc_audio_bus_0610 import patch as previous
    from patch_cyc_audio_bus_0610 import APU_SIGNATURE,MAPPER_SIGNATURE
    base='#include "hw_internal.h"\n'
    apu=previous(base+APU_SIGNATURE+'\n'+APU_OLD+'\n}\n','apu')
    mapper=previous(base+MAPPER_SIGNATURE+'\n','mapper')
    for kind,old in (('apu',apu),('mapper',mapper)):
        new=patch(old,kind)
        assert new.count(TAG)==1
        assert new.count('cviii_audio_bus_observe(hw.cycles, addr, value)')==1
        assert 'cviii_dmc_dma_trace_0650.h' in new
        if kind=='apu':
            assert new.count('hw_bus_read(apu.dmc_addr)')==1
            assert 'observed_phys=hw_cart.prg_off' in new
            assert 'apu.dmc_bytes' in new
        else:
            assert 'addr&0xF000u)==0xC000u' in new
        for damaged in (new,old.replace(INCLUDE,'// lost JP0610 include'),
                        old.replace(APU_OLD if kind=='apu' else MAPPER_OLD,
                                    '// corrupt original pinned observer source')):
            try:patch(damaged,kind)
            except ValueError:continue
            raise AssertionError("JP0650 accepted changed/duplicate pinned source")
    print("PASS JP0650 actual DMC bus-get and mapper24 bank-intent private overlays, no duplicate reads")

def main()->None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--smoke',action='store_true')
    parser.add_argument('--apu',type=Path)
    parser.add_argument('--mapper',type=Path)
    parser.add_argument('--out-apu',type=Path)
    parser.add_argument('--out-mapper',type=Path)
    a=parser.parse_args()
    if a.smoke:smoke();return
    if any(x is None for x in (a.apu,a.mapper,a.out_apu,a.out_mapper)):
        parser.error("need --apu --mapper --out-apu --out-mapper")
    apu=a.apu.resolve(strict=True)
    mapper=a.mapper.resolve(strict=True)
    outapu=a.out_apu.resolve()
    outmapper=a.out_mapper.resolve()
    if (apu.name!='cviii_hw_apu_audio.c' or
        mapper.name!='cviii_hw_mapper_fetch_audio.c' or
        outapu.name!='cviii_hw_apu_dma.c' or
        outmapper.name!='cviii_hw_mapper_dma.c' or
        len({apu.parent,mapper.parent,outapu.parent,outmapper.parent})!=1 or
        len({apu,outapu,mapper,outmapper})!=4):
        raise ValueError("JP0650 must use adjacent exact JP0610 private overlay files")
    apusource=patch(apu.read_text(encoding='utf-8'),'apu')
    mapsource=patch(mapper.read_text(encoding='utf-8'),'mapper')
    outapu.write_text(apusource,encoding='utf-8')
    outmapper.write_text(mapsource,encoding='utf-8')
    print("PASS JP0650 private APU DMA + mapper24 write-intent read-only hooks")

if __name__=='__main__':main()
