/* ROM-private, read-only whole-room geometry resource interface (v1).
 * Only the importer sees copyrighted Japanese ROM bytes. The owner-local
 * pack is not suitable for public distribution. No NES bus is modified. */
#ifndef CVIII_WHOLE_ROOM_H
#define CVIII_WHOLE_ROOM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CVIII_WHOLE_ROOM_COUNT 135u
#define CVIII_WHOLE_ROOM_HEADER 64u
#define CVIII_WHOLE_ROOM_STRIDE 32u

typedef struct {
    uint8_t stage, area, room, metadata, screen_count;
    uint8_t bg_r5, bg_r6, sprite_r2, sprite_r3;
    uint16_t width, height; /* 16x16 metatile coordinates, row-major */
    uint32_t source_rom_offset; /* provenance only, not native identity */
    const uint8_t *metatiles;   /* owned by pack, valid until close */
} CviiiWholeRoomView;

typedef struct {
    uint8_t *allocation;
    size_t allocation_size;
    CviiiWholeRoomView rooms[CVIII_WHOLE_ROOM_COUNT];
} CviiiWholeRoomPack;

/* Atomic replacement: unsuccessful load preserves existing pack. */
bool cviii_whole_room_open(CviiiWholeRoomPack *pack, const char *path);
void cviii_whole_room_close(CviiiWholeRoomPack *pack);
/* Pure validation, for tooling/tests; output is untouched on failure. */
bool cviii_whole_room_decode(const uint8_t *data, size_t len,
                            CviiiWholeRoomView out[CVIII_WHOLE_ROOM_COUNT]);
bool cviii_whole_room_find(const CviiiWholeRoomPack *pack,
                           uint8_t stage, uint8_t area, uint8_t room,
                           CviiiWholeRoomView *out);
bool cviii_whole_room_tile(const CviiiWholeRoomView *room,
                           unsigned x, unsigned y, uint8_t *tile);
#endif