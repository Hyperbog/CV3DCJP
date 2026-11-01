/* ROM-free C11 verification of the JP0598 native-index reader. */
#include "cviii_chr_index_0598.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    unsigned char *buffer = (unsigned char *)calloc(1, CVIII_CHR_INDEX_BYTES);
    if (!buffer) return 1;
    memcpy(buffer, "CVIII058", 8);
    for (size_t tile = 0; tile < CVIII_CHR_RAW_BYTES / 16; ++tile) {
        size_t raw = 8 + 16 * tile;
        size_t pixels = 8 + CVIII_CHR_RAW_BYTES + 64 * tile;
        buffer[raw] = 0x80;
        buffer[raw + 8] = 0x80;
        buffer[pixels] = 3;
    }
    if (!cviii_chr_index_check(buffer, CVIII_CHR_INDEX_BYTES, false) ||
        cviii_chr_index_check(buffer, CVIII_CHR_INDEX_BYTES, true)) return 2;
    buffer[8 + CVIII_CHR_RAW_BYTES] = 2;
    if (cviii_chr_index_check(buffer, CVIII_CHR_INDEX_BYTES, false)) return 3;
    buffer[8 + CVIII_CHR_RAW_BYTES] = 3;
    buffer[0] = 0;
    if (cviii_chr_index_check(buffer, CVIII_CHR_INDEX_BYTES, false)) return 4;
    free(buffer);
    printf("PASS JP0598 C11 native CHR index parity and synthetic identity refusal\n");
    return 0;
}
