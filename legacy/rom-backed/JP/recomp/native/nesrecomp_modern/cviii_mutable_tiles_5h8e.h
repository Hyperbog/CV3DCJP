#ifndef CVIII_MUTABLE_TILES_5H8E_H
#define CVIII_MUTABLE_TILES_5H8E_H
#include <stdbool.h>
#include <stdint.h>
/* Observed-only, room-visit tile-state overlay. Never changes emulated RAM/PPU. */
#define CVIII_MUT_SLOTS 256u
#define CVIII_MUT_TILE 8u
typedef bool (*CviiiStaticPixel5H8E)(void *ctx,unsigned x,unsigned y,uint32_t *out);
typedef struct {
    uint16_t x,y;
    uint8_t valid,confirmed,hits,unchanged;
    uint64_t observed_frame;
    uint32_t candidate[64],pixels[64];
} CviiiMutTile5H8E;
typedef struct {
    CviiiMutTile5H8E tiles[CVIII_MUT_SLOTS];
    uint8_t stage,area,room,active;
    uint64_t last_frame;
    unsigned committed, rejected_batches;
} CviiiMutableTiles5H8E;
void cviii_mut_reset_5h8e(CviiiMutableTiles5H8E *s);
/* A new room visit discards observations; the game decides reload persistence. */
void cviii_mut_visit_5h8e(CviiiMutableTiles5H8E *s,
                          uint8_t stage,uint8_t area,uint8_t room);
/* Capture only when the full native image has an independently validated
 * camera and the mapped palette/CHR state has not changed.  Sprite-covered
 * tiles and large-scale mismatches are rejected. */
void cviii_mut_observe_5h8e(CviiiMutableTiles5H8E *s,uint64_t frame,
    const uint32_t native[256*240],const uint8_t *oam,
    unsigned camera_x,unsigned camera_y,unsigned top,
    CviiiStaticPixel5H8E expected,void *ctx);
/* Paint ONLY outside the unchanged 256-pixel center. */
unsigned cviii_mut_paint_5h8e(const CviiiMutableTiles5H8E *s,uint32_t *canvas,
    unsigned width,unsigned height,unsigned native_x0,
    unsigned camera_x,unsigned camera_y,unsigned top);
unsigned cviii_mut_count_5h8e(const CviiiMutableTiles5H8E *s);
#endif
