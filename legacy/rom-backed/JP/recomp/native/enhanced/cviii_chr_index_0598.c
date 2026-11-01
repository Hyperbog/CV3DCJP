/* JP0598 native offline-asset reader. Purely presentation-owned data.
 * No ROM-derived instructions, no CPU/PPU writes, no external dependencies.
 * Identity digests here are FNV-1a64 cache-accident gates. Python separately
 * performs SHA256 validation of the full original-JP source/package.
 */
#include "cviii_chr_index_0598.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv64(const unsigned char *bytes, size_t length)
{
    uint64_t value = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < length; ++i) {
        value ^= bytes[i];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

bool cviii_chr_index_check(const unsigned char *data, size_t size, bool require_jp_identity)
{
    if (!data || size != CVIII_CHR_INDEX_BYTES || memcmp(data, "CVIII058", 8))
        return false;
    const unsigned char *raw = data + 8;
    const unsigned char *pixels = raw + CVIII_CHR_RAW_BYTES;
    if (require_jp_identity &&
        (fnv64(raw, CVIII_CHR_RAW_BYTES) != UINT64_C(0x282885aef688a342) ||
         fnv64(pixels, CVIII_CHR_PIXEL_BYTES) != UINT64_C(0x0c0ad42b16fc29dd)))
        return false;
    /* Every original planar CHR byte must reconstruct from the index. */
    for (size_t tile = 0; tile < CVIII_CHR_RAW_BYTES / 16; ++tile) {
        for (size_t row = 0; row < 8; ++row) {
            unsigned char lo = 0, hi = 0;
            for (size_t x = 0; x < 8; ++x) {
                unsigned char value = pixels[tile * 64 + row * 8 + x];
                if (value > 3) return false;
                lo |= (unsigned char)((value & 1) << (7 - x));
                hi |= (unsigned char)(((value >> 1) & 1) << (7 - x));
            }
            if (raw[tile * 16 + row] != lo || raw[tile * 16 + row + 8] != hi)
                return false;
        }
    }
    return true;
}

void cviii_chr_index_free(CviiiChrIndex *index)
{
    if (index) {
        free(index->allocation);
        index->allocation = NULL;
        index->raw = NULL;
        index->pixels = NULL;
    }
}

bool cviii_chr_index_load(CviiiChrIndex *index, const char *path)
{
    if (!index || !path || !*path) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    unsigned char *data = (unsigned char *)malloc(CVIII_CHR_INDEX_BYTES);
    if (!data) { fclose(file); return false; }
    size_t length = fread(data, 1, CVIII_CHR_INDEX_BYTES, file);
    int extra = fgetc(file);
    int io_error = ferror(file);
    fclose(file);
    if (length != CVIII_CHR_INDEX_BYTES || extra != EOF || io_error ||
        !cviii_chr_index_check(data, length, true)) {
        free(data);
        return false;
    }
    cviii_chr_index_free(index);
    index->allocation = data;
    index->raw = data + 8;
    index->pixels = data + 8 + CVIII_CHR_RAW_BYTES;
    return true;
}
