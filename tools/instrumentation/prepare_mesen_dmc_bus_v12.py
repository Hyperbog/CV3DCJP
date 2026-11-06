#!/usr/bin/env python3
"""Instrument only a clean private, pinned MesenCE source checkout for DMC bus evidence.

This patch is ROM-agnostic; the companion MesenCE Lua script strictly pins the
Japanese ROM. It does not touch any original installed emulator. It writes no
modified original cartridge or graphics. Compiling/running patched MesenCE
remains a separate owner-side action. Source verified 2026-11-05.
"""
import argparse
import subprocess
from pathlib import Path

COMMIT="4b8a196e299b3de297b204fd1208d2981562c89b"
BLOB="c6a88b5304c8b027154a1c6ced4d1bd07de2ccbb"
PATH="Core/NES/Debugger/NesDebugger.cpp"

# This method runs only for a true MemoryOperationType::DmaRead and has
# _cpu->IsDmcDma() from the CPU's actual DMA machine, unlike Lua callbacks.
ORIGINAL=b"\t\t\tbool isDmcDma = _cpu->IsDmcDma();"
INSTRUMENT=br'''
            /* CV3DCJP v12 debug-only, read-only DMC DMA evidence.
             * At this exact site the CPU itself positively identifies DMC,
             * not a normal bus read or a guessed APU sample-address change.
             * The CSV clock is NesCpu::GetCycleCount at debugger callback.
             * This hook is active ONLY when a private output path is set. */
            if(isDmcDma) {
                static bool cv3dcjpOpened = false;
                static std::FILE* cv3dcjpFile = nullptr;
                if(!cv3dcjpOpened) {
                    cv3dcjpOpened = true;
                    const char* path = std::getenv("CV3DCJP_MESEN_DMC_CSV");
                    if(path && *path) {
                        cv3dcjpFile = std::fopen(path, "wb");
                        if(!cv3dcjpFile) {
                            std::fprintf(stderr, "CV3DCJP: DMC CSV output failed\n");
                            std::abort();
                        }
                        std::fputs("cpu_cycle,address,value,cpu_pc,real_dmc_dma\n", cv3dcjpFile);
                        std::fflush(cv3dcjpFile);
                    }
                }
                if(cv3dcjpFile) {
                    std::fprintf(cv3dcjpFile, "%llu,%u,%u,%u,1\n",
                        (unsigned long long)_cpu->GetCycleCount(),
                        (unsigned)addr, (unsigned)value,
                        (unsigned)_cpu->GetState().PC);
                    std::fflush(cv3dcjpFile);
                }
            }
'''.strip(b'\n')


def run(repo):
    repo=repo.resolve()
    source=repo/PATH
    if not source.is_file():
        raise ValueError("Not a complete private MesenCE source tree")
    def git(*args):
        p=subprocess.run(["git","-C",str(repo),*args],
                         capture_output=True,text=True,check=True)
        return p.stdout.strip()
    if git("rev-parse","HEAD")!=COMMIT:
        raise ValueError("MesenCE source must be the exactly pinned commit "+COMMIT)
    if git("status","--porcelain"):
        raise ValueError("Private MesenCE source has changes; refusing to alter it")
    if git("rev-parse","HEAD:"+PATH)!=BLOB:
        raise ValueError("Pinned MesenCE source blob differs")
    if git("hash-object","--path="+PATH,str(source))!=BLOB:
        raise ValueError("On-disk file differs from authenticated Git blob")
    raw=source.read_bytes()
    if raw.count(ORIGINAL)!=1:
        raise ValueError("Exact original DMC discriminator not found once")
    if b"CV3DCJP_MESEN_DMC_CSV" in raw:
        raise ValueError("Already instrumented")
    if raw.count(b'#include "pch.h"')!=1:
        raise ValueError("Unexpected include structure")
    eol=b"\r\n" if b"\r\n" in raw else b"\n"
    block=INSTRUMENT.replace(b"\n",eol)
    patched=raw.replace(ORIGINAL,ORIGINAL+eol+block,1)
    patched=patched.replace(b'#include "pch.h"',
                           b'#include "pch.h"'+eol+b'#include <cstdio>'+eol+
                           b'#include <cstdlib>',1)
    source.write_bytes(patched)
    print("PATCHED private MesenCE source:",source)
    print("This is a SOURCE patch, not a compiled or runtime-verified emulator.")
    print("Only real IsDmcDma() memory reads are logged. Never interpret this")
    print("source patch as authorization to replace original 6502 instructions.")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-source",required=True,type=Path)
    a=p.parse_args()
    run(a.private_source)


if __name__=="__main__":
    main()
