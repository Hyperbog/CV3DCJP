#ifndef CVIII_DUAL_SCENE_5H8C_H
#define CVIII_DUAL_SCENE_5H8C_H
#include "cviii_room_renderer.h"
#include <stdint.h>
typedef struct {unsigned tested,matched,old_tested,old_matched,new_tested,new_matched;} CviiiDualScore5H8C;
/* Both sources are independently room-local. A split never establishes a world edge. */
CviiiDualScore5H8C cviii_dual_score_5h8c(const CviiiRoomRenderSource *old_scene,
    unsigned ox,unsigned oy,unsigned ot,const CviiiRoomRenderSource *new_scene,
    unsigned nx,unsigned ny,unsigned nt,unsigned split,int vertical,
    const uint32_t original[256*240],const uint8_t *oam);
int cviii_dual_qualified_5h8c(CviiiDualScore5H8C v);
unsigned cviii_dual_margins_5h8c(const CviiiRoomRenderSource *old_scene,
    unsigned ox,unsigned oy,unsigned ot,const CviiiRoomRenderSource *new_scene,
    unsigned nx,unsigned ny,unsigned nt,unsigned split,int vertical,
    uint32_t *dst,unsigned width,unsigned center);
#endif
