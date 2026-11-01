#ifndef CVIII_ROOM_STATE0_0658_H
#define CVIII_ROOM_STATE0_0658_H
#include <stdint.h>
#include <stddef.h>
#include "cviii_room_ppu_stream_0658.h"
typedef struct {
    uint8_t ram_after[2048];
    CviiiRoomPpuStream0658 ppu;
    uint8_t cpu_ram_bank_after, state_after;
    uint16_t mapper_reg_address, sound_reg_address;
    uint8_t mapper_reg_value, sound_reg_value;
    uint16_t distinct_cleared_bytes;
} CviiiRoomState0_0658;
/* Original-JP bank0A:B570-B58B + fixed E2C4,E616,E7B6, EB82,
 * E16C,E827,ED89. Output is a postcondition plan, NOT executed guest
 * writes or a cycle-accurate CPU state. 6502 stack page is unchanged here.
 * Input: original conditional room state $18=0D,$19=00,$23=0A.
 * On error returns -1 and preserves *out. */
int cviii_room_state0_0658(const uint8_t *raw0a,size_t raw_len,
    const uint8_t *fixed,size_t fixed_len,
    const uint8_t *ram,size_t ram_len,CviiiRoomState0_0658 *out);
#endif
