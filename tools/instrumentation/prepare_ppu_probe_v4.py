#!/usr/bin/env python3
"""Copy pinned NESRecomp source to a new checkout and add opt-in PPU/VRC6 tracing.

Original source is read-only. Destination must not exist. Anchors and SHA-256
are pinned to the owner's NESDev.zip; no game ROM or graphics are distributed.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import shutil

PINNED = {
    "hw_machine.c": "e5f2f1a72788f6c027c2215ce8db76cd3a9a631f02d591b15231ff05d2a4a925",
    "hw_ppu.c": "25f3dafc80928b7e5b93cc687d6f9fdeb2b426bf34bafaac6d5698b9b0547471",
    "hw_vrc6.inc": "f65e06e94230f8ef500ca9f48430fd24a34d67e991209da8133119cc1c663817",
}
HEADER = '''/* CV3DCJP-V4: observation only; no emulated-state mutation. */
#ifndef CVIII_PPU_PROBE_V4_H
#define CVIII_PPU_PROBE_V4_H
#include <stdint.h>
extern int cviii_v4_enabled;
void cviii_v4_emit(const char *kind, uint16_t cpu_address,
                   uint16_t ppu_address, unsigned slot,
                   uint32_t before, uint32_t after);
#endif
'''
ENGINE_IMPL = '''/* BEGIN CV3DCJP-V4 opt-in read-only observer. */
int cviii_v4_enabled;
static FILE *cviii_v4_file;
static unsigned long long cviii_v4_seq;
static void cviii_v4_init(void)
{
    const char *dest = getenv("CVIII_PPU_PROBE_V4");
    if (!dest || !*dest || cviii_v4_file) return;
    /* C11 exclusive creation: an earlier run is never overwritten. */
    cviii_v4_file = fopen(dest, "wx");
    if (!cviii_v4_file) {
        fprintf(stderr, "CV3DCJP-V4: cannot exclusively create event CSV\\n");
        return;
    }
    fputs("sequence,kind,cpu_cycle,ppu_scanline,ppu_dot,fetch_epoch,fetch_cycle,"
          "fetch_pc,fetch_prg_offset,cpu_address,ppu_address,slot,old_value,"
          "new_value,source_binding\\n", cviii_v4_file);
    fflush(cviii_v4_file);
    cviii_v4_enabled = 1;
}
void cviii_v4_emit(const char *kind, uint16_t cpu_addr, uint16_t ppu_addr,
                   unsigned slot, uint32_t before, uint32_t after)
{
    if (!cviii_v4_enabled || !cviii_v4_file) return;
    /* Async PPU commit's last CPU fetch does not prove the producer. */
    const char *binding = !strcmp(kind, "CPU_PPU_REQUEST") ||
                          !strcmp(kind, "CPU_VRC6_REQUEST") ?
                          "DIRECT_CPU_BUS_REQUEST" : "LAST_FETCH_CONTEXT_ONLY";
    fprintf(cviii_v4_file,
        "%llu,%s,%llu,%u,%u,%llu,%llu,%u,%lld,%u,%u,%u,%u,%u,%s\\n",
        ++cviii_v4_seq, kind, (unsigned long long)hw.cycles,
        (unsigned)ppu.scanline, (unsigned)ppu.dot,
        (unsigned long long)cviii_u_epoch,
        (unsigned long long)cviii_u_fetch_cycle,
        (unsigned)cviii_selector_fetch_pc_5h8s,
        (long long)cviii_selector_fetch_offset_5h8s,
        (unsigned)cpu_addr, (unsigned)ppu_addr, slot, before, after, binding);
    fflush(cviii_v4_file);
}
/* END CV3DCJP-V4 */
'''


def replace_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise ValueError(f"Expected one source anchor, found {text.count(old)}: {old[:70]!r}")
    return text.replace(old, new, 1)


def patch(name: str, source: str) -> str:
    if name == "hw_machine.c":
        source = replace_once(source, '#include "hw_internal.h"',
                              '#include "hw_internal.h"\n#include "cviii_ppu_probe_v4.h"')
        source = replace_once(source, "HwZapper hw_zapper;", ENGINE_IMPL + "\nHwZapper hw_zapper;")
        source = replace_once(source,
            "(cyc_cviii_selector_write_5h8s || cyc_cviii_selector_write_5h8u) && kind == HW_FETCH",
            "(cyc_cviii_selector_write_5h8s || cyc_cviii_selector_write_5h8u || cviii_v4_enabled) && kind == HW_FETCH")
        source = replace_once(source,
            "if (cyc_trace_enabled) cyc_trace_access(addr, value, true);\n    if (hw_cart.mapper==5",
            "if (cyc_trace_enabled) cyc_trace_access(addr, value, true);\n"
            "    if (cviii_v4_enabled) {\n"
            "        if (addr >= 0x2000 && addr < 0x4000)\n"
            '            cviii_v4_emit("CPU_PPU_REQUEST", addr, 0xffff, addr & 7u, 0, value);\n'
            "        if (hw_cart.mapper == 24 && addr >= 0x8000)\n"
            '            cviii_v4_emit("CPU_VRC6_REQUEST", addr, 0xffff, 0, 0, value);\n'
            "    }\n    if (hw_cart.mapper==5")
        source = replace_once(source, "memset(&hw, 0, sizeof(hw));",
                              "memset(&hw, 0, sizeof(hw));\n    cviii_v4_init();")
    elif name == "hw_ppu.c":
        source = replace_once(source, '#include "hw_internal.h"',
                              '#include "hw_internal.h"\n#include "cviii_ppu_probe_v4.h"')
        old = "ppu.palette[ppu.vbus & ((ppu.vbus & 3) ? 0x1F : 0x0F)] = value & 0x3F;"
        source = replace_once(source, old,
            "unsigned slot = ppu.vbus & ((ppu.vbus & 3) ? 0x1F : 0x0F);\n"
            "        uint8_t old = ppu.palette[slot];\n"
            "        " + old + "\n"
            '        cviii_v4_emit("PALETTE_COMMIT", 0xffff, ppu.vbus, slot, old, ppu.palette[slot]);')
        old = "for (int i = 0; i < 32; i++) ppu.palette[i] = c[i] & 0x3F;"
        source = replace_once(source, old,
            "for (int i = 0; i < 32; i++) {\n"
            "        uint8_t old = ppu.palette[i];\n"
            "        ppu.palette[i] = c[i] & 0x3F;\n"
            "        if (old != ppu.palette[i])\n"
            '            cviii_v4_emit("PALETTE_CORRUPTION", 0xffff, ppu.v, (unsigned)i, old, ppu.palette[i]);\n'
            "    }")
    elif name == "hw_vrc6.inc":
        source = replace_once(source, "        map_chr1(page, bank);",
            "        uint32_t previous = hw_cart.chr_off[page];\n"
            "        map_chr1(page, bank);\n"
            "        if (previous != hw_cart.chr_off[page])\n"
            '            cviii_v4_emit("VRC6_CHR_MAP", 0xffff, 0xffff, page,\n'
            "                          previous, hw_cart.chr_off[page]);")
        source = '#include "cviii_ppu_probe_v4.h"\n' + source
    else:
        raise ValueError(f"Unexpected module: {name}")
    return source


def prepare(source_root: Path, dest_root: Path, check_only: bool = False) -> dict:
    src = source_root.resolve(strict=True)
    dest = dest_root.resolve()
    if dest.exists() or src == dest or src in dest.parents or dest in src.parents:
        raise FileExistsError("Destination must be a new sibling, not the original source")
    base = src / "runner" / "cyc"
    pinned = {}
    for name, expected in PINNED.items():
        raw = (base / name).read_bytes()
        actual = hashlib.sha256(raw).hexdigest()
        if actual != expected:
            raise ValueError(f"Source SHA-256 mismatch for {name}: {actual}; expected {expected}")
        pinned[name] = patch(name, raw.decode("utf-8").replace("\r\n", "\n"))
    if not (src / "runner/cyc/project.cmake").is_file():
        raise FileNotFoundError("Missing engine project.cmake")
    if check_only:
        return {"status": "PINNED_ENGINE_SOURCE_VERIFIED", "files": list(pinned)}
    shutil.copytree(src, dest, symlinks=False, ignore=shutil.ignore_patterns(
        ".git", ".vs", "__pycache__", "build", "build-*", "cmake-build-*"))
    for name, content in pinned.items():
        (dest / "runner/cyc" / name).write_text(content, encoding="utf-8", newline="\n")
    (dest / "runner/cyc/cviii_ppu_probe_v4.h").write_text(
        HEADER, encoding="utf-8", newline="\n")
    return {"status": "ISOLATED_ENGINE_COPY_PATCHED", "destination": str(dest),
            "patched": sorted(pinned)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    import json
    print(json.dumps(prepare(args.source, args.destination, args.check_only), indent=2))


if __name__ == "__main__":
    main()
