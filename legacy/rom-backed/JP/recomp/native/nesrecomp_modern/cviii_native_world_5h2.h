/* Private index from 5H-1 JSON: original packs supply the actual tile data.
 * This is a read-only presentation facility, not a new gameplay/PPU engine. */
#ifndef CVIII_NATIVE_WORLD_5H2_H
#define CVIII_NATIVE_WORLD_5H2_H
#include <stdbool.h>
#include <stdint.h>
#include "cviii_whole_room.h"
#include "cviii_room_gfx.h"
#include "cviii_room_renderer.h"
#define CVIII_WORLD_INDEX_ROOMS 135u
typedef struct {
    uint8_t stage, area, room, group, placed;
    int32_t x, y;
    uint16_t width, height; /* original metatiles: each is 32 px */
    uint32_t source_rom_offset;
    uint64_t layout_digest;
} CviiiWorldEntry5H2;
typedef struct {
    CviiiWorldEntry5H2 entries[CVIII_WORLD_INDEX_ROOMS];
    bool loaded;
} CviiiNativeWorld5H2;
/* Transactional: rejects missing or altered assets and unverifiable placement. */
bool cviii_world_index_open_5h2(CviiiNativeWorld5H2 *world, const char *path,
                                const CviiiWholeRoomPack *rooms,
                                const CviiiRoomGfx *graphics);
const CviiiWorldEntry5H2 *cviii_world_index_find_5h2(
    const CviiiNativeWorld5H2 *world,uint8_t stage,uint8_t area,uint8_t room);
/* Paint existing authenticated world rooms at a verified global camera.
 * Only extra side strips are changed. Does not repeat unplaced geometry. */
uint32_t cviii_world_margins_5h2(const CviiiNativeWorld5H2 *world,
    const CviiiWholeRoomPack *rooms,const CviiiRoomRenderSource *active,
    unsigned camera_x,unsigned camera_y,unsigned top,
    uint32_t *output,unsigned width,unsigned height,unsigned native_x);
/* 5H-5: signed global camera can occupy verified preceding room at transitions. */
uint32_t cviii_world_margins_at_5h5(const CviiiNativeWorld5H2 *world,
    const CviiiWholeRoomPack *rooms,const CviiiRoomRenderSource *active,
    int32_t world_x,int32_t world_y,unsigned top,
    uint32_t *output,unsigned width,unsigned height,unsigned native_x);
#endif
