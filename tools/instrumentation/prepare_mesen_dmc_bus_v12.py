#!/usr/bin/env python3
"""Add read-only, explicit DMA bus records to a CLEAN pinned private MesenCE checkout.

All actual NES debugger DmaRead/DmaWrite operations are recorded. Only
_cpu->IsDmcDma() is allowed to label a DMC sample read; ordinary Lua
callbacks cannot distinguish that operation. This is never patched into
the user's original Mesen installation.
"""
import argparse
from pathlib import Path
import subprocess

PIN="4b8a196e299b3de297b204fd1208d2981562c89b"
ORIGINAL_BLOB="c6a88b5304c8b027154a1c6ced4d1bd07de2ccbb"
SOURCE="Core/NES/Debugger/NesDebugger.cpp"
INCLUDE=b'#include "pch.h"'
READ_ANCHOR=b'\t\t\tbool isDmcDma = _cpu->IsDmcDma();'
WRITE_ANCHOR=b'\tInstructionProgress.LastMemOperation = operation;'
# The write method's anchor is shared with ProcessRead: restrict replacement to
# the suffix after the unambiguous ProcessWrite function signature.
WRITE_FUNCTION=b'void NesDebugger::ProcessWrite(uint32_t addr, uint8_t value, MemoryOperationType type)'

HELPER=r'''
/* CV3DCJP v12: private opt-in cycle evidence. No emulator bus writes.
 * Cycle is observed from the real NES CPU debugger, before the Lua callback.
 * DMC=1 only in the actual NesCpu::IsDmcDma read interval.
 */
static void cv3dcjp_v12_dma_bus(unsigned long long cycle,
                                unsigned addr, unsigned value,
                                unsigned pc, char kind, unsigned dmc)
{
    static bool opened = false;
    static std::FILE *out = nullptr;
    if(!opened) {
        /* An explicitly recorded Lua reset is required. Never mix the
         * startup/pre-reset CPU cycle epoch with the synchronized capture. */
        const char *armPath = std::getenv("CV3DCJP_MESEN_ARM_FILE");
        if(!armPath || !*armPath) return;
        std::FILE *armed = std::fopen(armPath, "rb");
        if(!armed) return;
        std::fclose(armed);
        opened = true;
        const char *path = std::getenv("CV3DCJP_MESEN_DMC_CSV");
        if(path && *path) {
            out = std::fopen(path, "wb");
            if(!out) {
                std::fprintf(stderr, "CV3DCJP: cannot create private DMC CSV\n");
                std::abort();
            }
            std::fputs("cpu_cycle,bus_kind,address,value,cpu_pc,real_dmc_sample_read\n", out);
            std::fflush(out);
        }
    }
    if(out) {
        std::fprintf(out, "%llu,%c,%u,%u,%u,%u\n",
                     cycle, kind, addr, value, pc, dmc);
        std::fflush(out);
    }
}
'''.strip("\n")
READ_INSERT=r'''
            cv3dcjp_v12_dma_bus(
                (unsigned long long)_cpu->GetCycleCount(),
                (unsigned)addr, (unsigned)value,
                (unsigned)_cpu->GetState().PC, 'R', isDmcDma ? 1u : 0u);
'''.strip("\n")
WRITE_INSERT=r'''
    if(type == MemoryOperationType::DmaWrite) {
        cv3dcjp_v12_dma_bus(
            (unsigned long long)_cpu->GetCycleCount(),
            (unsigned)addr, (unsigned)value,
            (unsigned)_cpu->GetState().PC, 'W', 0u);
    }
'''.strip("\n")

def git(repo,*cmd):
    return subprocess.run(['git','-C',str(repo),*cmd],text=True,
                          capture_output=True,check=True).stdout.strip()

def patch_source(repo):
    repo=repo.resolve()
    file=repo/SOURCE
    if not file.is_file():raise ValueError("Not a MesenCE source checkout")
    if git(repo,'rev-parse','HEAD')!=PIN:
        raise ValueError("Wrong MesenCE revision; pinned commit: "+PIN)
    if git(repo,'status','--porcelain'):
        raise ValueError("Source has uncommitted changes; private patch refused")
    if git(repo,'rev-parse','HEAD:'+SOURCE)!=ORIGINAL_BLOB:
        raise ValueError("MesenCE pinned source blob changed")
    if git(repo,'hash-object','--path='+SOURCE,str(file))!=ORIGINAL_BLOB:
        raise ValueError("Working source does not match pinned Git blob")
    raw=file.read_bytes()
    eol=b'\r\n' if b'\r\n' in raw else b'\n'
    def to_bytes(s):
        return s.encode('utf-8').replace(b'\n',eol)
    if raw.count(INCLUDE)!=1 or raw.count(READ_ANCHOR)!=1 or raw.count(WRITE_FUNCTION)!=1:
        raise ValueError("Source hooks do not match authenticated MesenCE revision")
    if b'cv3dcjp_v12_dma_bus' in raw:
        raise ValueError("Already instrumented")
    # Do not touch original includes or machine behavior other than observation.
    helper=(b'#include <cstdio>'+eol+b'#include <cstdlib>'+eol+
            b'#include <cstdint>'+eol+eol+to_bytes(HELPER)+eol)
    raw=raw.replace(INCLUDE,INCLUDE+eol+helper,1)
    raw=raw.replace(READ_ANCHOR,READ_ANCHOR+eol+to_bytes(READ_INSERT),1)
    head,tail=raw.split(WRITE_FUNCTION,1)
    if tail.count(WRITE_ANCHOR)!=1:
        raise ValueError("Unexpected write method structure")
    tail=tail.replace(WRITE_ANCHOR,WRITE_ANCHOR+eol+to_bytes(WRITE_INSERT),1)
    file.write_bytes(head+WRITE_FUNCTION+tail)
    print("PRIVATE MesenCE source patched:",file)
    print("CSV event type comes from real DmaRead/DmaWrite and IsDmcDma();")
    print("not a fabricated Lua operation type. Build and runtime still pending.")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--private-source',required=True,type=Path)
    a=p.parse_args()
    patch_source(a.private_source)

if __name__=='__main__':
    main()
