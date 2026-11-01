/* JP0653: non-mutating, byte-source-derived native 6502 pointer lookup.
 * Caller authenticates source ROM; tests compare original opcode execution. */
#include "cviii_phrase_lookup_0653.h"

#define BASE 0x8000u
#define START 0xA304u
#define TABLE_END 0xA42Cu
#define POOL_BEGIN 0xA42Cu
#define POOL_END 0xA957u
#define BANK_SIZE 16384u

static uint16_t word_at(const uint8_t *bank, unsigned cpu) {
    unsigned off = cpu - BASE;
    return (uint16_t)((unsigned)bank[off] | ((unsigned)bank[off + 1u] << 8u));
}

int cviii_phrase_lookup_0653(const uint8_t *bank, size_t len,
                             int mode, uint8_t selector,
                             CviiiPhraseLookup0653 *out) {
    uint16_t pointer[148], start, end;
    unsigned i, index, ordinal, first, j;
    CviiiPhraseLookup0653 result = {0};
    if (!bank || !out || len != BANK_SIZE || (mode != 0 && mode != 1))
        return -1;
    /* Validate entire indexed table and its bounded phrase spans before
     * trusting one selector. No original game data are embedded here. */
    for (i = 0; i < 148u; ++i) {
        pointer[i] = word_at(bank, START + 2u*i);
        if (pointer[i] < POOL_BEGIN || pointer[i] >= POOL_END ||
            (i && pointer[i] < pointer[i-1u]))
            return -1;
    }
    if (pointer[0] != POOL_BEGIN || pointer[147] >= POOL_END)
        return -1;
    for (i = 0; i < 148u; ++i) {
        if (i && pointer[i] == pointer[i-1u])
            continue;
        end = POOL_END;
        for (j = i+1u; j < 148u; ++j)
            if (pointer[j] > pointer[i]) { end = pointer[j]; break; }
        if (end <= pointer[i] || bank[(unsigned)end - BASE - 1u] != 0xFFu)
            return -1;
    }
    /* NMOS 6502: ASL A discards bit7 before TAY. Indexed read uses the
     * same 8-bit Y for both original LDA abs,Y instructions. */
    index = (unsigned)(selector & 0x7Fu);
    first = (mode == 0 ? START : 0xA404u) + 2u*index;
    result.source_word_cpu_address = (uint16_t)first;
    result.final_y = (uint8_t)(selector << 1u);
    result.original_raw_word = word_at(bank, first);
    result.final_a = (uint8_t)(result.original_raw_word >> 8u);
    result.final_n = (uint8_t)((result.final_a & 0x80u) != 0u);
    result.final_z = (uint8_t)(result.final_a == 0u);
    result.source_is_indexed_table = (uint8_t)(mode == 0 || index < 20u);
    if (!result.source_is_indexed_table) {
        *out = result;
        return 0;  /* Original CPU reads still happen; semantics not proven. */
    }
    ordinal = mode == 0 ? index : 128u + index;
    start = pointer[ordinal];
    end = POOL_END;
    for (i = ordinal+1u; i < 148u; ++i)
        if (pointer[i] > start) { end = pointer[i]; break; }
    result.span_start = start;
    result.span_end_exclusive = end;
    *out = result;
    return 1;
}
