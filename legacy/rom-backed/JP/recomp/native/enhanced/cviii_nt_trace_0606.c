/* JP0606: actual PPU nametable/attribute bytes returned during a fetch.
 * Diagnostic metadata only. No substituted pixels, registers or memory.
 */
#include "cviii_nt_trace_0606.h"
#include <inttypes.h>
#include <string.h>

#define FNV_INIT UINT64_C(14695981039346656037)
#define FNV_PRIME UINT64_C(1099511628211)
static CviiiNtTrace *s_observer;
static uint64_t mix(uint64_t h, uint8_t byte) {
    return (h ^ byte) * FNV_PRIME;
}

void cviii_nt_observe(CviiiNtTrace *trace, uint16_t addr, uint8_t value,
                      uint8_t rom_source, uint16_t scanline, uint16_t dot)
{
    if (!trace || !trace->active) return;
    if (scanline >= CVIII_NT_LINES || dot > 340u ||
        addr < 0x2000u || addr >= 0x4000u || (addr & 0x3fffu) >= 0x3f00u ||
        rom_source > 1u) {
        ++trace->total_invalid;
        if (scanline < CVIII_NT_LINES) ++trace->lines[scanline].invalid;
        return;
    }
    CviiiNtLine *line = &trace->lines[scanline];
    unsigned tile_offset = addr & 1023u;
    if (tile_offset >= 960u) {
        ++line->attrs;
        ++trace->total_attrs;
        if (line->first_attr < 0) line->first_attr = value;
    } else {
        ++line->tiles;
        ++trace->total_tiles;
        if (line->first_tile < 0) line->first_tile = value;
    }
    if (rom_source) {
        ++line->chr_rom;
        ++trace->total_rom;
    } else ++line->ciram;
    /* Preserve address, byte, bus source and exact timing in the hash.
     * Different raster changes can therefore yield distinct line hashes. */
    uint64_t hash = line->rolling_hash;
    hash = mix(hash, (uint8_t)addr);
    hash = mix(hash, (uint8_t)(addr >> 8));
    hash = mix(hash, value);
    hash = mix(hash, rom_source);
    hash = mix(hash, (uint8_t)dot);
    hash = mix(hash, (uint8_t)(dot >> 8));
    line->rolling_hash = hash;
}

static void from_ppu(uint16_t addr, uint8_t value, uint8_t rom_source,
                     uint16_t line, uint16_t dot)
{
    cviii_nt_observe(s_observer, addr, value, rom_source, line, dot);
}

bool cviii_nt_open(CviiiNtTrace *trace, const char *path)
{
    if (!trace || !path || !*path) return false;
    FILE *output = fopen(path, "w");
    if (!output) return false;
    cviii_nt_close(trace);
    memset(trace, 0, sizeof(*trace));
    trace->output = output;
    fputs("frame,scanline,tile_reads,attribute_reads,ciram_reads,chrrom_reads,"
          "invalid,first_tile,first_attribute,bus_fnv64\n", output);
    return true;
}

void cviii_nt_close(CviiiNtTrace *trace)
{
    if (!trace) return;
    if (s_observer == trace) {
        cviii_nt_set_hook(NULL);
        s_observer = NULL;
    }
    if (trace->output) fclose(trace->output);
    trace->output = NULL;
    trace->active = 0;
}

void cviii_nt_begin(CviiiNtTrace *trace, bool sampled, uint64_t frame)
{
    if (!trace || !trace->output || !sampled) return;
    trace->frames = frame;
    for (unsigned i = 0; i < CVIII_NT_LINES; ++i) {
        CviiiNtLine *line = &trace->lines[i];
        memset(line, 0, sizeof(*line));
        line->first_tile = line->first_attr = -1;
        line->rolling_hash = FNV_INIT;
    }
    trace->active = 1u;
    s_observer = trace;
    cviii_nt_set_hook(from_ppu);
}

void cviii_nt_end(CviiiNtTrace *trace)
{
    if (!trace || !trace->active) return;
    cviii_nt_set_hook(NULL);
    s_observer = NULL;
    trace->active = 0u;
    ++trace->sample_frames;
    bool emitted = false;
    for (unsigned i = 0; i < CVIII_NT_LINES; ++i) {
        const CviiiNtLine *line = &trace->lines[i];
        if (!line->tiles && !line->attrs && !line->invalid) continue;
        emitted = true;
        fprintf(trace->output, "%" PRIu64 ",%u,%u,%u,%u,%u,%u,%d,%d,%016" PRIx64 "\n",
                trace->frames, i, line->tiles, line->attrs, line->ciram,
                line->chr_rom, line->invalid, line->first_tile,
                line->first_attr, line->rolling_hash);
    }
    /* During title fades/rendering-off frames the original PPU performs no
     * nametable fetches. Preserve one explicit empty sample so this trace
     * still aligns with the simultaneous JP0601 frame fingerprint. */
    if (!emitted)
        fprintf(trace->output, "%" PRIu64 ",0,0,0,0,0,0,-1,-1,%016" PRIx64 "\n",
                trace->frames, FNV_INIT);
    fflush(trace->output);
}
