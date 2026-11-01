/* JP0598 read-only authentic-JP CHR cache; never write NES hardware state. */
#ifndef CVIII_CHR_INDEX_0598_H
#define CVIII_CHR_INDEX_0598_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CVIII_CHR_RAW_BYTES 131072u
#define CVIII_CHR_PIXEL_BYTES 524288u
#define CVIII_CHR_INDEX_BYTES (8u + CVIII_CHR_RAW_BYTES + CVIII_CHR_PIXEL_BYTES)
typedef struct {
    unsigned char *allocation;
    const unsigned char *raw;
    const unsigned char *pixels;
} CviiiChrIndex;
bool cviii_chr_index_check(const unsigned char *data, size_t size, bool require_jp_identity);
bool cviii_chr_index_load(CviiiChrIndex *index, const char *path);
void cviii_chr_index_free(CviiiChrIndex *index);
#endif
