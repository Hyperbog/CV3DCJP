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

LEGACY_HELPER=r'''
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
# Preserve exact v12 legacy bytes for safe owner-side repairs. No manual edits.
HELPER = LEGACY_HELPER.replace("        const char *armPath = std::getenv(\"CV3DCJP_MESEN_ARM_FILE\");\n        if(!armPath || !*armPath) return;\n        std::FILE *armed = std::fopen(armPath, \"rb\");\n        if(!armed) return;\n        std::fclose(armed);\n        opened = true;\n        const char *path = std::getenv(\"CV3DCJP_MESEN_DMC_CSV\");\n        if(path && *path) {\n            out = std::fopen(path, \"wb\");\n            if(!out) {", "        /* Checked, size-limited environment values and MSVC secure CRT:\n         * Core.vcxproj enables /WX, so deprecated getenv/fopen are invalid. */\n        char armPath[4096] = {};\n        size_t armSize = 0;\n        if(getenv_s(&armSize, armPath, sizeof(armPath),\n                    \"CV3DCJP_MESEN_ARM_FILE\") != 0 || armSize <= 1)\n            return;\n        std::FILE *armed = nullptr;\n        if(fopen_s(&armed, armPath, \"rb\") != 0 || !armed) return;\n        std::fclose(armed);\n        opened = true;\n        char path[4096] = {};\n        size_t pathSize = 0;\n        if(getenv_s(&pathSize, path, sizeof(path),\n                    \"CV3DCJP_MESEN_DMC_CSV\") == 0 && pathSize > 1) {\n            if(fopen_s(&out, path, \"wb\") != 0 || !out) {")
if HELPER == LEGACY_HELPER:
    raise RuntimeError("MesenCE secure CRT helper replacement missing")
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

def checked_original(repo):
    """Return pinned upstream source, never a source guess."""
    original=subprocess.run(
        ["git","-C",str(repo),"show","HEAD:"+SOURCE],
        capture_output=True,check=True).stdout
    if git(repo,"rev-parse","HEAD:"+SOURCE)!=ORIGINAL_BLOB:
        raise ValueError("Original upstream MesenCE source blob mismatch")
    return original

def instrumented(original,helper):
    """Recreate exactly the legacy or secure-CRT source patch."""
    eol=b'\r\n' if b'\r\n' in original else b'\n'
    def to_bytes(s):
        return s.encode('utf-8').replace(b'\n',eol)
    if (original.count(INCLUDE)!=1 or original.count(READ_ANCHOR)!=1
        or original.count(WRITE_FUNCTION)!=1):
        raise ValueError("Authenticated MesenCE hook anchors changed")
    helpers=(b'#include <cstdio>'+eol+b'#include <cstdlib>'+eol+
             b'#include <cstdint>'+eol+eol+to_bytes(helper)+eol)
    patched=original.replace(INCLUDE,INCLUDE+eol+helpers,1)
    patched=patched.replace(READ_ANCHOR,
                            READ_ANCHOR+eol+to_bytes(READ_INSERT),1)
    head,tail=patched.split(WRITE_FUNCTION,1)
    if tail.count(WRITE_ANCHOR)!=1:
        raise ValueError("Unexpected authenticated MesenCE write method")
    tail=tail.replace(WRITE_ANCHOR,
                      WRITE_ANCHOR+eol+to_bytes(WRITE_INSERT),1)
    return head+WRITE_FUNCTION+tail

def patch_source(repo,repair=False):
    repo=repo.resolve()
    file=repo/SOURCE
    if not file.is_file():
        raise ValueError("Not a MesenCE source checkout")
    if git(repo,'rev-parse','HEAD')!=PIN:
        raise ValueError("Wrong MesenCE revision; pinned commit: "+PIN)
    original=checked_original(repo)
    current=file.read_bytes()
    if repair:
        # Git's blob can contain LF while a Windows checkout has CRLF due to
        # core.autocrlf. Reproduce *both* authenticated working-tree layouts.
        # Never guess or rewrite EOLs; accept only an exact known legacy patch.
        original_candidates=[original]
        if b'\r\n' not in original:
            original_candidates.append(original.replace(b'\n',b'\r\n'))
        if any(current==instrumented(x,HELPER) for x in original_candidates):
            print("Already repaired: pinned private MesenCE DMC hook uses secure CRT")
            return
        # No patch is made if the owner made any OTHER tracked modifications.
        modified=git(repo,'diff','--name-only').splitlines()
        staged=git(repo,'diff','--cached','--name-only').splitlines()
        if modified!=[SOURCE] or staged:
            raise ValueError("Private MesenCE has unexpected tracked changes: "+
                             repr({'unstaged':modified,'staged':staged}))
        matches=[x for x in original_candidates if current==instrumented(x,LEGACY_HELPER)]
        if len(matches)!=1:
            raise ValueError("Existing private source is not the EXACT legacy "+
                             "v12 patch; refusing partial/unknown edits")
        file.write_bytes(instrumented(matches[0],HELPER))
        print("REPAIRED exact legacy private MesenCE DMC instrumentation in-place.")
        print("Replaced MSVC C4996 getenv/fopen with checked getenv_s/fopen_s.")
        print("No original emulator installation or other sources modified.")
        return
    if git(repo,'status','--porcelain'):
        raise ValueError("Source has uncommitted changes; private patch refused")
    # Authentic Windows Git checkouts may materialize CRLF from a LF blob.
    original_worktree = original
    if current!=original:
        if b'\r\n' not in original and current==original.replace(b'\n',b'\r\n'):
            original_worktree=current
        else:
            raise ValueError("Working source does not equal pinned upstream Git blob")
    file.write_bytes(instrumented(original_worktree,HELPER))
    print("PATCHED private pinned MesenCE source:",file)
    print("Source-identified DMC bus logging uses checked MSVC secure CRT.")
    print("Build/runtime bus parity remains unverified.")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--private-source',required=True,type=Path)
    p.add_argument('--repair',action='store_true',
                   help='Safely upgrade only EXACT earlier v12 patched source')
    a=p.parse_args()
    patch_source(a.private_source,repair=a.repair)

if __name__=='__main__':
    main()
