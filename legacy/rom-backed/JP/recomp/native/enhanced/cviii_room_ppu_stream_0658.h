#ifndef CVIII_ROOM_PPU_STREAM_0658_H
#define CVIII_ROOM_PPU_STREAM_0658_H
#include <stdint.h>
#include <stddef.h>
#define CVIII_0658_MAX_WRITES 2048u
/* A read-only bank-qualified extraction of the original PRG31 EB82-EBF6
 * room nametable clear stream. NO automatic game/PPU/mapper changes. */
typedef struct {
    uint16_t address[CVIII_0658_MAX_WRITES];
    uint8_t value[CVIII_0658_MAX_WRITES];
    uint16_t count, final_vram_address, final_source_pointer;
    uint8_t ctrl_setup, ctrl_final, mask_setup;
    uint8_t initial_ppu_address_high, initial_ppu_address_low;
    uint8_t second_ppu_address_high, second_ppu_address_low;
    uint8_t address_switch_count, original_ppu_status_reads;
    uint8_t consumed_stream_bytes, scratch02_final_count;
} CviiiRoomPpuStream0658;
/* full fixed physical 8K bank31, and original $FF PPUCTRL / $FE PPUMASK;
 * PPUSTATUS must be externally proven to give the clear-latch behavior.
 * On failure (-1) *out is unchanged. On success (1), return the complete
 * ordered PPUDATA transfer, not a fake full-frame rendered image. */
int cviii_room_ppu_stream_0658(const uint8_t *fixed31, size_t size,
 uint8_t ff, uint8_t fe, CviiiRoomPpuStream0658 *out);
#endif
