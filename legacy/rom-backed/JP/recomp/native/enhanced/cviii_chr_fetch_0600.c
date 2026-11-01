/* JP0600: read-only PPU CHR fetch audit for VRC6a Japanese cartridge.
 * This does NOT replace the bytes the PPU reads. It records actual reads,
 * physical ROM offsets, mapper-bank changes per PPU address window and
 * verifies those bytes against the owner-authenticated raw/decoded CHR cache.
 */
#include "cviii_chr_fetch_0600.h"
#include <stddef.h>
#include <string.h>

static CviiiChrFetchAudit *s_current;

static uint8_t cached_planar_byte(const CviiiChrIndex *asset, uint32_t physical)
{
    size_t tile = physical / 16u, plane = physical & 15u;
    size_t row = plane & 7u;
    unsigned result = 0;
    for (unsigned x = 0; x < 8; ++x) {
        uint8_t color = asset->pixels[tile * 64 + row * 8 + x];
        result |= ((color >> (plane >= 8)) & 1u) << (7u - x);
    }
    return (uint8_t)result;
}

void cviii_chr_fetch_observe(CviiiChrFetchAudit *a, uint16_t addr,
                             uint32_t physical, uint8_t value,
                             uint16_t line, uint16_t dot)
{
    if (!a || !a->active || !a->asset || !a->asset->raw ||
        !a->asset->pixels) return;
    ++a->total_reads;
    if (line >= CVIII_FETCH_LINES || dot > 340u ||
        addr >= 0x2000u || physical >= CVIII_CHR_RAW_BYTES) {
        ++a->total_invalid;
        if (line < CVIII_FETCH_LINES) ++a->lines[line].invalid;
        return;
    }
    CviiiChrFetchLine *sample = &a->lines[line];
    ++sample->reads;
    unsigned page = (addr >> 10) & 7u;
    unsigned bank = physical >> 10;
    if (sample->first_bank[page] == 255u)
        sample->first_bank[page] = (uint8_t)bank;
    else if (sample->last_bank[page] != bank) {
        if (sample->bank_changes[page] != UINT16_MAX)
            ++sample->bank_changes[page];
    }
    sample->last_bank[page] = (uint8_t)bank;
    if (a->asset->raw[physical] != value ||
        cached_planar_byte(a->asset, physical) != value) {
        ++sample->bad;
        ++a->total_bad;
    }
}

static void from_mapper(uint16_t addr, uint32_t physical, uint8_t value,
                        uint16_t line, uint16_t dot)
{
    cviii_chr_fetch_observe(s_current, addr, physical, value, line, dot);
}

bool cviii_chr_fetch_open(CviiiChrFetchAudit *audit, const char *path)
{
    if (!audit || !path || !*path) return false;
    FILE *file = fopen(path, "w");
    if (!file) return false;
    cviii_chr_fetch_close(audit);
    memset(audit, 0, sizeof(*audit));
    audit->output = file;
    fputs("frame,scanline,fetches,asset_mismatch,invalid,"
          "ppu0_first,ppu0_last,ppu0_changes,"
          "ppu1_first,ppu1_last,ppu1_changes,"
          "ppu2_first,ppu2_last,ppu2_changes,"
          "ppu3_first,ppu3_last,ppu3_changes,"
          "ppu4_first,ppu4_last,ppu4_changes,"
          "ppu5_first,ppu5_last,ppu5_changes,"
          "ppu6_first,ppu6_last,ppu6_changes,"
          "ppu7_first,ppu7_last,ppu7_changes\n", file);
    return true;
}

void cviii_chr_fetch_close(CviiiChrFetchAudit *audit)
{
    if (!audit) return;
    if (s_current == audit) {
        cviii_chr_fetch_set_hook(NULL);
        s_current = NULL;
    }
    if (audit->output) fclose(audit->output);
    audit->output = NULL;
    audit->active = 0;
}

void cviii_chr_fetch_frame_begin(CviiiChrFetchAudit *audit,
                                 const CviiiChrIndex *asset)
{
    if (!audit || !audit->output || !asset || !asset->raw || !asset->pixels)
        return;
    ++audit->frames;
    if ((audit->frames - 1u) % 60u != 0u) return;
    audit->asset = asset;
    audit->sample_start_reads = audit->total_reads;
    audit->sample_start_bad = audit->total_bad;
    audit->sample_start_invalid = audit->total_invalid;
    audit->active = 1;
    for (unsigned i = 0; i < CVIII_FETCH_LINES; ++i) {
        CviiiChrFetchLine *line = &audit->lines[i];
        memset(line, 0, sizeof(*line));
        memset(line->first_bank, 255, sizeof(line->first_bank));
        memset(line->last_bank, 255, sizeof(line->last_bank));
    }
    s_current = audit;
    cviii_chr_fetch_set_hook(from_mapper);
}

void cviii_chr_fetch_frame_end(CviiiChrFetchAudit *audit)
{
    if (!audit || !audit->active) return;
    cviii_chr_fetch_set_hook(NULL);
    s_current = NULL;
    audit->active = 0;
    ++audit->sample_frames;
    for (unsigned line = 0; line < CVIII_FETCH_LINES; ++line) {
        const CviiiChrFetchLine *sample = &audit->lines[line];
        if (!sample->reads && !sample->invalid) continue;
        fprintf(audit->output, "%llu,%u,%u,%u,%u",
                (unsigned long long)audit->frames, line,
                sample->reads, sample->bad, sample->invalid);
        for (unsigned page = 0; page < 8; ++page) {
            unsigned first = sample->first_bank[page];
            unsigned last = sample->last_bank[page];
            if (first == 255u) fputs(",-1,-1,0", audit->output);
            else fprintf(audit->output, ",%u,%u,%u", first, last,
                         sample->bank_changes[page]);
        }
        fputc('\n', audit->output);
    }
    fflush(audit->output);
}
