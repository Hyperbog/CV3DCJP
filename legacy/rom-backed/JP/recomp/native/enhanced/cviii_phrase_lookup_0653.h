/* JP0653: bounded original Japanese raw0C phrase-pointer source lookup.
 * Source evidence only: no opcode/phrase interpretation or live audio. */
#ifndef CVIII_PHRASE_LOOKUP_0653_H
#define CVIII_PHRASE_LOOKUP_0653_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t final_a, final_y, final_z, final_n;
    uint8_t source_is_indexed_table;
    uint16_t source_word_cpu_address;
    uint16_t original_raw_word; /* Unverified outside indexed table. */
    uint16_t span_start, span_end_exclusive; /* 0 when unverified. */
} CviiiPhraseLookup0653;
/* mode 0: original $8B2C reads $01B7,X, indexed from $A304.
 * mode 1: original $8D07 reads ZP $06, indexed from $A404.
 * The original ASL A wraps; input is an 8-bit value.
 * In both cases opcode-read addresses may extend into data; this
 * implementation NEVER labels those latter reads as phrase pointers.
 * Returns 1 on a structurally bounded phrase; 0 for out-of-table
 * reads; -1 for invalid input or structurally corrupt source. */
int cviii_phrase_lookup_0653(const uint8_t *raw0c, size_t bank_size,
                             int mode, uint8_t selector,
                             CviiiPhraseLookup0653 *out);
#endif
