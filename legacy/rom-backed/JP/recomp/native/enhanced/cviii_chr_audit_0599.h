/* JP0599 read-only VRC6 CHR register mapping and per-frame audit. */
#ifndef CVIII_CHR_AUDIT_0599_H
#define CVIII_CHR_AUDIT_0599_H
#include "cviii_chr_index_0598.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    unsigned ctrl, regs[8], offsets[8], banks[8];
} CviiiChrBanks;

typedef struct {
    FILE *out;
    unsigned interval;
    uint64_t frames;
    uint64_t samples;
    uint64_t mismatch;
} CviiiChrAudit;

/* Exact lower-level mapper snapshot format from pinned NESRecomp hw_mapper.c;
 * no direct HwCart/PPU structure access and no guest memory writes. */
bool cviii_chr_banks_read(FILE *snapshot, CviiiChrBanks *out);
bool cviii_chr_banks_check(CviiiChrBanks *banks);
bool cviii_chr_audit_open(CviiiChrAudit *audit, const char *path);
void cviii_chr_audit_close(CviiiChrAudit *audit);
void cviii_chr_audit_frame(CviiiChrAudit *audit, const CviiiChrIndex *index,
                           FILE *snapshot, const uint8_t *palette);
#endif
