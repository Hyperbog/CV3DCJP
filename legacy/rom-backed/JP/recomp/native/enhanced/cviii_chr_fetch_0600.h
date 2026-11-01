/* JP0600: optional, observation-only VRC6 physical pattern-fetch audit. */
#ifndef CVIII_CHR_FETCH_0600_H
#define CVIII_CHR_FETCH_0600_H
#include "cviii_chr_index_0598.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef void (*CviiiChrFetchHook)(uint16_t ppu_addr, uint32_t physical,
                                  uint8_t returned_byte, uint16_t line, uint16_t dot);

/* Defined ONLY by the private external hw_mapper.c overlay. A NULL callback
 * leaves stock mapper code/PPU values authoritative. */
void cviii_chr_fetch_set_hook(CviiiChrFetchHook callback);

#define CVIII_FETCH_LINES 312
typedef struct {
    uint32_t reads, bad, invalid;
    uint8_t first_bank[8], last_bank[8];
    uint16_t bank_changes[8];
} CviiiChrFetchLine;

typedef struct {
    FILE *output;
    const CviiiChrIndex *asset;
    uint64_t frames, sample_frames, total_reads, total_bad, total_invalid;
    uint64_t sample_start_reads, sample_start_bad, sample_start_invalid;
    CviiiChrFetchLine lines[CVIII_FETCH_LINES];
    unsigned active;
} CviiiChrFetchAudit;

bool cviii_chr_fetch_open(CviiiChrFetchAudit *audit, const char *path);
void cviii_chr_fetch_close(CviiiChrFetchAudit *audit);
void cviii_chr_fetch_frame_begin(CviiiChrFetchAudit *audit, const CviiiChrIndex *asset);
void cviii_chr_fetch_frame_end(CviiiChrFetchAudit *audit);
/* A direct test feed exercises exactly the callback semantics used by the
 * private runtime hook; never reads or changes NES hardware. */
void cviii_chr_fetch_observe(CviiiChrFetchAudit *audit,
                             uint16_t ppu_addr, uint32_t physical,
                             uint8_t returned_byte, uint16_t line, uint16_t dot);
#endif
