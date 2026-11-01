#ifndef CVIII_ROOM_COORDINATE_0657_H
#define CVIII_ROOM_COORDINATE_0657_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint8_t a, x, y, carry, zero, negative, overflow;
    uint8_t room_hi_73, room_sub_08, room_offset_09, masked_0e, room_phase_0a;
} CviiiRoomCoordinate0657;
/* Conditional original PRG raw0A:$80D1-$8107 (JSR $8001), read-only.
 * Caller must authenticate the 16KiB original raw0A bank and its mapping.
 * This models final scalar state, not 6502 cycle/stack/PPU behavior.
 * Returns 1 or -1; errors never change *out. */
int cviii_room_coordinate_0657(const uint8_t *bank, size_t bank_len,
        const uint8_t *ram, size_t ram_len, CviiiRoomCoordinate0657 *out);
#endif
