/* JP0601 synchronized ORIGINAL PPU outcome (not Enhanced compositor).
 * FNV-1a64 of explicitly serialized 9-bit PPU index, bg-opaque mask and
 * scanline status. These are compact comparisons, not reconstructed images
 * or an argument that cached tile bytes imply final pixel parity.
 */
#include "cviii_frame_evidence_0601.h"
#include <inttypes.h>
#include <string.h>

#define HASH_INIT UINT64_C(14695981039346656037)
#define HASH_PRIME UINT64_C(1099511628211)

static uint64_t mix(uint64_t h, uint8_t byte)
{
    return (h ^ byte) * HASH_PRIME;
}

bool cviii_frame_evidence_open(CviiiFrameEvidence *audit, const char *path)
{
    if (!audit || !path || !*path) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;
    cviii_frame_evidence_close(audit);
    memset(audit, 0, sizeof(*audit));
    audit->output = f;
    fputs("frame,pattern_reads,cache_mismatches,invalid_reads,bank_changes,"
          "ppu_color_fnv64,bg_opaque_fnv64,scanline_state_fnv64,"
          "nonzero_bg_pixels,visible_bg_lines,visible_sprite_lines\n", f);
    return true;
}

void cviii_frame_evidence_close(CviiiFrameEvidence *audit)
{
    if (!audit) return;
    if (audit->output) fclose(audit->output);
    audit->output = NULL;
}

bool cviii_frame_evidence_sample(CviiiFrameEvidence *audit,
                                 const CviiiChrFetchAudit *fetch,
                                 const uint16_t *frame_index,
                                 const uint8_t *bg_opaque,
                                 const uint8_t line_bytes[CVIII_FRAME_LINES_BYTES])
{
    if (!audit || !audit->output || !fetch || !fetch->active)
        return false;
    if (!frame_index || !bg_opaque || !line_bytes) {
        ++audit->input_failures;
        return false;
    }
    uint64_t color_hash = HASH_INIT, bg_hash = HASH_INIT, lines_hash = HASH_INIT;
    uint32_t opaque_pixels = 0u, bg_lines = 0u, sprite_lines = 0u;
    uint64_t transitions = 0u;
    for (unsigned i = 0; i < CVIII_FRAME_PIXELS; ++i) {
        uint16_t index = frame_index[i];
        if (index > 511u || bg_opaque[i] > 1u) {
            ++audit->input_failures;
            return false;
        }
        color_hash = mix(mix(color_hash, (uint8_t)index), (uint8_t)(index >> 8));
        bg_hash = mix(bg_hash, bg_opaque[i]);
        opaque_pixels += bg_opaque[i];
    }
    for (unsigned i = 0; i < CVIII_FRAME_LINES_BYTES; ++i)
        lines_hash = mix(lines_hash, line_bytes[i]);
    for (unsigned y = 0; y < CVIII_FRAME_HEIGHT; ++y) {
        uint8_t mask = line_bytes[y * 5u + 4u];
        if (mask & 0x08u) ++bg_lines;
        if (mask & 0x10u) ++sprite_lines;
    }
    for (unsigned y = 0; y < CVIII_FETCH_LINES; ++y)
        for (unsigned page = 0; page < 8; ++page)
            transitions += fetch->lines[y].bank_changes[page];
    uint64_t reads = fetch->total_reads - fetch->sample_start_reads;
    uint64_t bad = fetch->total_bad - fetch->sample_start_bad;
    uint64_t invalid = fetch->total_invalid - fetch->sample_start_invalid;
    fprintf(audit->output, "%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
            ",%" PRIu64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64
            ",%u,%u,%u\n",
            fetch->frames, reads, bad, invalid, transitions,
            color_hash, bg_hash, lines_hash, opaque_pixels, bg_lines, sprite_lines);
    fflush(audit->output);
    ++audit->samples;
    return true;
}
