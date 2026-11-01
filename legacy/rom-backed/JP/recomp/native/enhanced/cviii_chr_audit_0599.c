/* JP0599: observe live VRC6 CHR register bank mapping at FRAME-END only.
 * Per-scanline/instruction bank switching is not reconstructed here.
 * No mapper writes; no hardware memory or controller reads with side effects.
 */
#include "cviii_chr_audit_0599.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

bool cviii_chr_banks_read(FILE *snapshot, CviiiChrBanks *out)
{
    if (!snapshot || !out) return false;
    memset(out, 0, sizeof(*out));
    rewind(snapshot);
    unsigned have_ctrl = 0, have_regs = 0, have_offsets = 0;
    char line[256];
    while (fgets(line, sizeof(line), snapshot)) {
        unsigned index, value;
        if (sscanf(line, "cart.ctrl %x", &value) == 1) {
            if (have_ctrl || value > 255) return false;
            out->ctrl = value;
            have_ctrl = 1;
        } else if (sscanf(line, "cart.vrc_chr[%u] %x", &index, &value) == 2) {
            if (index >= 8 || (have_regs & (1u << index)) || value > 511)
                return false;
            out->regs[index] = value;
            have_regs |= 1u << index;
        } else if (sscanf(line, "cart.chr_off[%u] %x", &index, &value) == 2) {
            if (index >= 8 || (have_offsets & (1u << index)) ||
                value >= CVIII_CHR_RAW_BYTES || (value & 1023u))
                return false;
            out->offsets[index] = value;
            have_offsets |= 1u << index;
        }
    }
    return have_ctrl && have_regs == 255u && have_offsets == 255u &&
           !ferror(snapshot);
}

bool cviii_chr_banks_check(CviiiChrBanks *banks)
{
    if (!banks) return false;
    unsigned mode = banks->ctrl & 3u;
    bool matched = true;
    for (unsigned page = 0; page < 8; ++page) {
        bool pair = mode == 1u || (mode >= 2u && page >= 4u);
        unsigned reg = mode == 1u ? page / 2u :
                       pair ? 4u + (page - 4u) / 2u : page;
        unsigned bank = banks->regs[reg];
        if (pair && (banks->ctrl & 0x20u))
            bank = (bank & ~1u) | (page & 1u);
        /* Japanese VRC6a: 128 physical 1 KiB CHR-ROM pages.
         * Bank numbers alias by modulo 128 (hardware address pins). */
        banks->banks[page] = bank & 127u;
        if (banks->offsets[page] != banks->banks[page] * 1024u)
            matched = false;
    }
    return matched;
}

bool cviii_chr_audit_open(CviiiChrAudit *audit, const char *path)
{
    if (!audit || !path || !*path) return false;
    FILE *log = fopen(path, "w");
    if (!log) return false;
    cviii_chr_audit_close(audit);
    memset(audit, 0, sizeof(*audit));
    audit->out = log;
    audit->interval = 60;
    fprintf(log, "sample,frame,mode,ctrl,bank0,bank1,bank2,bank3,"
                 "bank4,bank5,bank6,bank7,palette32,mapper_match\n");
    return true;
}

void cviii_chr_audit_close(CviiiChrAudit *audit)
{
    if (!audit) return;
    if (audit->out) fclose(audit->out);
    audit->out = NULL;
}

void cviii_chr_audit_frame(CviiiChrAudit *audit, const CviiiChrIndex *index,
                           FILE *snapshot, const uint8_t *palette)
{
    if (!audit || !audit->out || !index || !index->raw || !index->pixels ||
        !snapshot || !palette) return;
    CviiiChrBanks banks;
    bool parsed = cviii_chr_banks_read(snapshot, &banks);
    bool matched = parsed && cviii_chr_banks_check(&banks);
    if (!matched) ++audit->mismatch;
    ++audit->samples;
    fprintf(audit->out, "%llu,%llu,%u,%02X",
            (unsigned long long)audit->samples,
            (unsigned long long)audit->frames,
            parsed ? banks.ctrl & 3u : 0u,
            parsed ? banks.ctrl : 0u);
    for (unsigned page = 0; page < 8; ++page) {
        if (parsed) {
            /* Bounds already checked; never read beyond authenticated cache. */
            unsigned idx = banks.banks[page];
            if (idx >= CVIII_CHR_RAW_BYTES / 1024u) matched = false;
            fprintf(audit->out, ",%u", idx);
        } else fputs(",-1", audit->out);
    }
    fputc(',', audit->out);
    for (unsigned i = 0; i < 32; ++i)
        fprintf(audit->out, "%02X", palette[i]);
    fprintf(audit->out, ",%s\n", matched ? "PASS" : "FAIL");
    fflush(audit->out);
}
