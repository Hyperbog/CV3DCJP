/* JP0606: read-only VRC6 PPU nametable / attribute fetch audit. */
#ifndef CVIII_NT_TRACE_0606_H
#define CVIII_NT_TRACE_0606_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CVIII_NT_LINES 312u
typedef void (*CviiiNtHook)(uint16_t addr, uint8_t value, uint8_t rom_source,
                            uint16_t scanline, uint16_t dot);
/* Provided by the private external hw_ppu.c overlay. NULL disables all work.
 * The hook never alters the PPU byte, CIRAM, mapper or IRQ state. */
void cviii_nt_set_hook(CviiiNtHook callback);

typedef struct {
    uint32_t tiles, attrs, ciram, chr_rom, invalid;
    uint64_t rolling_hash;
    int first_tile, first_attr;
} CviiiNtLine;
typedef struct {
    FILE *output;
    uint64_t frames, sample_frames, total_tiles, total_attrs;
    uint64_t total_rom, total_invalid;
    CviiiNtLine lines[CVIII_NT_LINES];
    unsigned active;
} CviiiNtTrace;

bool cviii_nt_open(CviiiNtTrace *trace, const char *path);
void cviii_nt_close(CviiiNtTrace *trace);
void cviii_nt_begin(CviiiNtTrace *trace, bool sampled, uint64_t frame);
void cviii_nt_end(CviiiNtTrace *trace);
void cviii_nt_observe(CviiiNtTrace *trace, uint16_t addr, uint8_t value,
                      uint8_t rom_source, uint16_t scanline, uint16_t dot);
#endif
