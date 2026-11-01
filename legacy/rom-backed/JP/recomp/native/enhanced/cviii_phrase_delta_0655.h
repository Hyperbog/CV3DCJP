/* JP0655: authenticated raw PRG0C original 6502 $8D15-$8D8F ordinary
 * phrase-byte arithmetic. Pure C11, no game sample bytes, no audio writes.
 * All CPU addresses require caller-proven 16KiB raw bank 0C mapping. */
#ifndef CVIII_PHRASE_DELTA_0655_H
#define CVIII_PHRASE_DELTA_0655_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint8_t source_byte;
    uint8_t entry_x, exit_x, exit_y, exit_a, exit_c, exit_z, exit_n, exit_v;
    uint8_t cursor_after, high_nibble_after, scratch_e2_after;
    uint8_t low_ec_after, high_ed_after, did_update_high_nibble;
    uint16_t source_cpu_address;
} CviiiPhraseDelta0655;
/* Returns 1 for byte <$FB, 0 for $FB-$FF, -1 if input/source invalid.
 * On 0 or -1, *out is unchanged; the original CPU's command-token branch
 * (>= $FB) is deliberately NOT implemented or declared harmless.
 * The function models only the scalar state postcondition, not RAM writes,
 * stack contents, cycle timing, mapper events or VRC6/APU hardware. */
int cviii_phrase_delta_0655(const uint8_t *raw0c, size_t bank_size,
                            const uint8_t *ram, size_t ram_size,
                            uint8_t entry_x, CviiiPhraseDelta0655 *out);
#endif
