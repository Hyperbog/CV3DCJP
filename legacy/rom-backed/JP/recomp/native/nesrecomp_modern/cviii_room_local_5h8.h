/* 5H-8: strict room-local fallback. No inferred inter-room coordinates. */
#ifndef CVIII_ROOM_LOCAL_5H8_H
#define CVIII_ROOM_LOCAL_5H8_H
#include "cviii_room_renderer.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct { unsigned tested,matched; } CviiiLocalScore5H8;
typedef struct {
    uint64_t frame;
    uint8_t stage,area,room;
    int scroll;
    int32_t camera;
    bool valid;
} CviiiLocalTracker5H8;
void cviii_local_reset_5h8(CviiiLocalTracker5H8 *track);
/* Up to 8 hypotheses in *room-local* pixel coordinates. */
unsigned cviii_local_seeds_5h8(const CviiiLocalTracker5H8 *track,
    uint64_t frame,const CviiiWholeRoomView *room,int scroll,
    unsigned ram_hint,unsigned out[8]);
void cviii_local_confirm_5h8(CviiiLocalTracker5H8 *track,
    uint64_t frame,const CviiiWholeRoomView *room,int scroll,unsigned camera);
CviiiLocalScore5H8 cviii_local_match_5h8(const CviiiRoomRenderSource *src,
    unsigned camera_x,unsigned camera_y,unsigned top,
    const uint32_t *native,const uint8_t *oam);
/* Paint ONLY pixels covered by the selected ROM-authenticated room.
 * Caller prepopulates matte margins and copies the original center. */
unsigned cviii_local_margins_5h8(const CviiiRoomRenderSource *src,
    unsigned camera_x,unsigned camera_y,unsigned top,uint32_t *output,
    unsigned width,unsigned height,unsigned center_x);
#endif
