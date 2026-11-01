#ifndef CVIII_TRANSITION_5H8D_H
#define CVIII_TRANSITION_5H8D_H
#include "cviii_dual_scene_5h8c.h"
#include <stdbool.h>
#include <stdint.h>
/* A camera hypothesis for the outgoing room, never a world-graph edge. */
typedef struct {
    uint64_t start,frame;
    uint8_t stage,area,room;
    int64_t x,y;
    int sx,sy;
    bool valid;
} CviiiTransitionTrack5H8D;
void cviii_transition_reset_5h8d(CviiiTransitionTrack5H8D *t);
bool cviii_transition_step_5h8d(CviiiTransitionTrack5H8D *t,
    uint64_t last_verified_frame,uint64_t frame,
    uint8_t stage,uint8_t area,uint8_t room,
    int initial_x,int initial_y,int old_scroll_x,int old_scroll_y,
    int new_scroll_x,int new_scroll_y,bool vertical,
    int64_t *out_x,int64_t *out_y);
typedef struct {
    CviiiDualScore5H8C score;
    unsigned split,direction;
} CviiiTransitionPlan5H8D;
/* Direction: 0 old-left, 1 old-above, 2 old-right, 3 old-below.
 * Samples each original room only once per candidate camera, then searches
 * 16-pixel split positions. Every admitted split uses the unchanged 5H8C
 * 90-sample/96%-agreement gate, independently for both contributing regions. */
CviiiTransitionPlan5H8D cviii_transition_best_5h8d(
    const CviiiRoomRenderSource *old_scene,unsigned ox,unsigned oy,unsigned ot,
    const CviiiRoomRenderSource *new_scene,unsigned nx,unsigned ny,unsigned nt,
    const uint32_t native[256*240],const uint8_t oam[256]);
/* Render both margins from the SAME four-way split chosen by the pixel gate.
 * This is distinct from legacy cviii_dual_margins_5h8c(vertical:boolean):
 * direction 2 and 3 mean old-right and old-below, not simply vertical. */
unsigned cviii_transition_paint_5h8d(
    const CviiiRoomRenderSource *old_scene,unsigned ox,unsigned oy,unsigned ot,
    const CviiiRoomRenderSource *new_scene,unsigned nx,unsigned ny,unsigned nt,
    CviiiTransitionPlan5H8D plan,uint32_t *dst,unsigned width,unsigned center);
#endif
