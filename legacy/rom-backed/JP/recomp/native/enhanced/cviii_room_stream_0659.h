#ifndef CVIII_ROOM_STREAM_0659_H
#define CVIII_ROOM_STREAM_0659_H
#include <stddef.h>
#include <stdint.h>
#define CVIII_0659_MAPPER_MAX 114u
#define CVIII_0659_MAX_RECORDS 10u
/* Original JP VRC6 physical raw0A:$B7F7-$B872, with original fixed
 * PRG31:$FB96-$FBA4 source-reader ABI. Conditional entry only.
 * State is read-only to the caller. No CPU cycles or real mapper writes. */
typedef struct {
    uint8_t ram_after[2048];
    uint16_t source_root;
    uint8_t record_count;
    uint8_t source_y_after, x_after, a_after;
    uint8_t carry_after, zero_after, negative_after, overflow_after;
    uint8_t bank_after;
    uint8_t source_calls;
    uint8_t mapper_events;
    uint8_t mapper_values[CVIII_0659_MAPPER_MAX];
} CviiiRoomStream0659;
/* Return 1 for a physically source-bounded selector 0..4 with a complete
 * FF-terminated original five-byte record stream, 0 for unsupported selector,
 * -1 for malformed buffers/table/stream/context. Neither 0 nor -1 mutates out.
 * Original 6502 parent-stack return and cycle budget are NOT simulated. */
int cviii_room_stream_0659(const uint8_t *raw0a, size_t len0a,
    const uint8_t *raw0e, size_t len0e,
    const uint8_t *ram, size_t ram_len, uint8_t incoming_overflow,
    CviiiRoomStream0659 *out);
#endif
