#include "cviii_world_parity_5h5.h"
#include <stddef.h>
#include <string.h>

bool cviii_world_pixel_5h5(const CviiiNativeWorld5H2 *w,
    const CviiiWholeRoomPack *rooms,const CviiiRoomRenderSource *s,
    int64_t wx,int64_t wy,uint32_t *argb,uint8_t *other)
{
    if(!w||!w->loaded||!rooms||!s||!s->room||!s->graphics||
       !s->chr||!argb||!other||wx<0||wy<0)return false;
    const CviiiGfxGroup *base=cviii_room_gfx_group(s->graphics,s->room);
    if(!base)return false;
    for(unsigned i=0;i<CVIII_WORLD_INDEX_ROOMS;i++) {
        const CviiiWorldEntry5H2 *e=&w->entries[i];
        if(!e->placed||e->group!=base->id || wx<e->x || wy<e->y ||
           wx-(int64_t)e->x>=(int64_t)e->width*32 ||
           wy-(int64_t)e->y>=(int64_t)e->height*32)continue;
        CviiiWholeRoomView room;
        if(!cviii_whole_room_find(rooms,e->stage,e->area,e->room,&room) ||
           cviii_room_gfx_group(s->graphics,&room)!=base)continue;
        CviiiGfxPixel pixel;
        if(!cviii_room_gfx_sample(s->graphics,&room,s->chr,s->bg_bank_1k,
              (unsigned)(wx-e->x),(unsigned)(wy-e->y),&pixel) ||
           !cviii_room_gfx_color(&pixel,s->palette_argb,argb))return false;
        *other=(uint8_t)(room.stage!=s->room->stage||
                         room.area!=s->room->area||room.room!=s->room->room);
        return true;
    }
    return false;
}
static bool covered_by_sprite(const uint8_t *oam,unsigned x,unsigned y)
{
    if(!oam)return false;
    for(unsigned slot=0;slot<64;slot++) {
        const uint8_t *o=oam+slot*4;
        if(o[0]>=0xefu)continue;
        unsigned sy=(unsigned)o[0]+1u;
        if(x>=(unsigned)o[3] && x<(unsigned)o[3]+8u &&
           y>=sy && y<sy+16u)return true;
    }
    return false;
}
CviiiWorldMatch5H5 cviii_world_match_5h5(
    const CviiiNativeWorld5H2 *w,const CviiiWholeRoomPack *rooms,
    const CviiiRoomRenderSource *s,int64_t wx,int64_t wy,unsigned top,
    const uint32_t native[256*240],const uint8_t *oam)
{
    CviiiWorldMatch5H5 r={0};
    if(!native||top>=240||!s)return r;
    for(unsigned y=64;y<=224;y+=16u) {
        if(y<top)continue;
        for(unsigned x=16;x<=240;x+=16u) {
            if(covered_by_sprite(oam,x,y))continue;
            uint32_t expected=0;uint8_t other=0;
            if(!cviii_world_pixel_5h5(w,rooms,s,wx+x,wy+(int64_t)y-top,
                                       &expected,&other))continue;
            r.tested++;r.crossed+=other;
            r.matched+=(expected==native[(size_t)y*256u+x]);
        }
    }
    return r;
}
bool cviii_world_seam_candidate_5h5(const CviiiNativeWorld5H2 *w,
    const CviiiWholeRoomView *selected,int scroll,int32_t *camera_x)
{
    if(!w||!w->loaded||!selected||!camera_x || selected->stage!=0 ||
       selected->area!=1 || selected->room!=0 ||
       (selected->metadata&0x80u)||scroll<0||scroll>256)return false;
    const CviiiWorldEntry5H2 *a=cviii_world_index_find_5h2(w,0,0,0);
    const CviiiWorldEntry5H2 *b=cviii_world_index_find_5h2(w,0,1,0);
    if(!a||!b||!a->placed||!b->placed||a->x+a->width*32!=b->x||
       a->y!=b->y||a->height!=b->height)return false;
    *camera_x=b->x-256+scroll;
    return true;
}
