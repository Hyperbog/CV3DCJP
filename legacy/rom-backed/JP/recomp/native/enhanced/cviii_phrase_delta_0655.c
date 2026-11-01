/* JP0655: source-derived native semantic lift of original PRG0C
 * $8D15-$8D8F for ordinary command bytes $00-$FA. The four original
 * routines are the caller $8D15, repeat-add $8D4C, add $8D60 and
 * subtract $8D76. Hardware and executable control tokens are excluded. */
#include "cviii_phrase_delta_0655.h"

int cviii_phrase_delta_0655(const uint8_t *bank, size_t bank_len,
                            const uint8_t *ram, size_t ram_len,
                            uint8_t x, CviiiPhraseDelta0655 *out) {
    CviiiPhraseDelta0655 state = {0};
    uint16_t ptr, source;
    uint8_t cursor, token, nibble, factor, magnitude, restored;
    uint8_t oldlo, oldhi, newlo, newhi, carry, high_updated;
    unsigned temp;
    if (!bank || bank_len != 16384u || !ram || ram_len != 2048u || !out)
        return -1;
    cursor = ram[0x06AAu + x];
    ptr = (uint16_t)((unsigned)ram[0xE2u] | ((unsigned)ram[0xE3u] << 8));
    source = (uint16_t)(ptr + cursor);  /* original (ZP $E2),Y */
    if (source < 0x8000u || source >= 0xC000u)
        return -1; /* No unproven bank, RAM, or I/O bus abstraction. */
    token = bank[source - 0x8000u];
    if (token >= 0xFBu)
        return 0; /* Original special-token handler begins at $8D90. */
    nibble = (uint8_t)(token & 15u);
    factor = ram[0x03F7u + x];
    /* The 6502 $8D4C repeat-add loop uses an 8-bit X counter.
     * Factor 0 means 256 iterations, whose 8-bit sum is zero. */
    magnitude = (uint8_t)((unsigned)(nibble < 8u ? nibble : 16u-nibble) * factor);
    restored = ram[0xEEu];
    oldlo = ram[0x01ADu + restored];
    oldhi = ram[0x01B2u + restored];
    if (nibble < 8u) {
        temp = (unsigned)oldlo + magnitude;
        newlo = (uint8_t)temp;
        carry = (uint8_t)(temp > 255u);
        newhi = (uint8_t)(oldhi + carry);
        high_updated = carry;
        state.exit_v = (uint8_t)((~(magnitude ^ oldlo) & (magnitude ^ newlo) & 0x80u) != 0u);
    } else {
        newlo = (uint8_t)(oldlo - magnitude);
        carry = (uint8_t)(oldlo >= magnitude);
        high_updated = (uint8_t)!carry;
        newhi = (uint8_t)(oldhi - high_updated);
        state.exit_v = (uint8_t)(((oldlo ^ magnitude) & (oldlo ^ newlo) & 0x80u) != 0u);
    }
    state.source_byte = token;
    state.source_cpu_address = source;
    state.entry_x = x;
    state.exit_x = restored; /* Original LDX $EE after multiply loop. */
    state.exit_y = cursor;
    state.exit_a = oldhi;    /* Final LDA $01B2,X; STA $ED */
    state.exit_c = carry;
    /* Final INC/DEC $ED updates N/Z iff carry/borrow requires it.
     * Otherwise N/Z are from the final LDA high byte. */
    state.exit_z = (uint8_t)((high_updated ? newhi : oldhi) == 0u);
    state.exit_n = (uint8_t)((high_updated ? newhi : oldhi) >> 7u);
    state.cursor_after = (uint8_t)(cursor + 1u);
    state.high_nibble_after = token >= 0x10u ?
        (uint8_t)(token >> 4u) : ram[0x03F2u + x];
    state.did_update_high_nibble = (uint8_t)(token >= 0x10u);
    state.scratch_e2_after = magnitude;
    state.low_ec_after = newlo;
    state.high_ed_after = newhi;
    *out = state;
    return 1;
}
