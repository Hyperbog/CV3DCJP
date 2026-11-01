#ifndef CVIII_PHRASE_CONTROL_0656_H
#define CVIII_PHRASE_CONTROL_0656_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint8_t token, kind, parameter;
    uint8_t a, x, y, carry, zero, negative, overflow;
    uint8_t cursor, repeat_start, repeat_counter, high_nibble;
    uint8_t write_cursor, write_repeat_start, write_repeat_counter, write_high_nibble;
    uint16_t token_cpu, next_pc;
} CviiiPhraseControl0656;
/* Source bank 0C must be authenticated by the caller. RAM is 2KiB.
 * The helper returns a trace-typed result; it does not write guest RAM.
 * 1 = control opcode, 0 = ordinary opcode not handled, -1 = bad input/bus.
 * kind 1: $FB-$FD cursor transfer; 2: $FE repeat ends;
 *      3: $FE repeat back edge; 4: $FF channel end.
 * All errors and ordinary opcodes leave output untouched. */
int cviii_phrase_control_0656(const uint8_t *bank, size_t bank_len,
    const uint8_t *ram, size_t ram_len, uint8_t x,
    uint8_t incoming_overflow, CviiiPhraseControl0656 *out);
#endif
