/* 5H-5: ROM-private, read-only cross-room camera parity. */
#ifndef CVIII_WORLD_PARITY_5H5_H
#define CVIII_WORLD_PARITY_5H5_H
#include "cviii_native_world_5h2.h"
#include <stdint.h>
typedef struct {
    unsigned tested, matched, crossed;
} CviiiWorldMatch5H5;
/* False for unknown geometry, incompatible graphics, or unplaced worlds. */
bool cviii_world_pixel_5h5(const CviiiNativeWorld5H2 *world,
    const CviiiWholeRoomPack *rooms,const CviiiRoomRenderSource *source,
    int64_t world_x,int64_t world_y,uint32_t *argb,uint8_t *other_room);
/* Native pixel sampling only; includes one-pixel offsets, no tile rounding. */
CviiiWorldMatch5H5 cviii_world_match_5h5(
    const CviiiNativeWorld5H2 *world,const CviiiWholeRoomPack *rooms,
    const CviiiRoomRenderSource *source,int64_t camera_world_x,
    int64_t camera_world_y,unsigned top,const uint32_t native[256*240],
    const uint8_t *oam);
/* Verified 0/0/0 -> 0/1/0 seam: candidate = seam - 256 + live scroll.
 * This is a parity-gated hypothesis, not automatic proof of camera accuracy. */
bool cviii_world_seam_candidate_5h5(const CviiiNativeWorld5H2 *world,
    const CviiiWholeRoomView *selected,int ppu_scroll,int32_t *world_camera_x);
#endif
