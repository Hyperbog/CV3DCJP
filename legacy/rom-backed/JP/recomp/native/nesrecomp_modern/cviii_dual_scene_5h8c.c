#include "cviii_dual_scene_5h8c.h"
#include <stdbool.h>
#include <stddef.h>
static bool actor(const uint8_t *oam,unsigned x,unsigned y) {
    if(!oam)return false;
    for(unsigned i=0;i<64;i++) {
        const uint8_t *o=oam+i*4;
        if(o[0]>=239)continue;
        if(x>=o[3]&&x<(unsigned)o[3]+8&&y>(unsigned)o[0]&&y<=(unsigned)o[0]+16)return true;
    }
    return false;
}
static bool get(const CviiiRoomRenderSource *s,int64_t x,int64_t y,uint32_t *c) {
    if(!s||!s->room||!s->graphics||!s->chr||!c||x<0||y<0||
       x>=(int64_t)s->room->width*32||y>=(int64_t)s->room->height*32)return false;
    CviiiGfxPixel px;
    return cviii_room_gfx_sample(s->graphics,s->room,s->chr,s->bg_bank_1k,
        (unsigned)x,(unsigned)y,&px)&&cviii_room_gfx_color(&px,s->palette_argb,c);
}
static bool scene(const CviiiRoomRenderSource *s,unsigned cx,unsigned cy,
                  unsigned top,int x,unsigned y,uint32_t *c){
    if(y<top)return false;
    return get(s,(int64_t)cx+x,(int64_t)cy+y-top,c);
}
CviiiDualScore5H8C cviii_dual_score_5h8c(const CviiiRoomRenderSource *old_scene,
    unsigned ox,unsigned oy,unsigned ot,const CviiiRoomRenderSource *new_scene,
    unsigned nx,unsigned ny,unsigned nt,unsigned split,int vertical,
    const uint32_t native[256*240],const uint8_t *oam){
    CviiiDualScore5H8C v={0};
    if(!native||!old_scene||!new_scene||split<16||split>224)return v;
    for(unsigned y=64;y<=224;y+=16)for(unsigned x=16;x<=240;x+=16){
        if(actor(oam,x,y))continue;
        const bool old=vertical==0?x<split:vertical==1?y<split:vertical==2?x>=split:y>=split;
        uint32_t c;
        if(!scene(old?old_scene:new_scene,old?ox:nx,old?oy:ny,
                  old?ot:nt,(int)x,y,&c))continue;
        const unsigned hit=(c==native[(size_t)y*256+x]);
        v.tested++;v.matched+=hit;
        if(old){v.old_tested++;v.old_matched+=hit;}
        else {v.new_tested++;v.new_matched+=hit;}
    }
    return v;
}
int cviii_dual_qualified_5h8c(CviiiDualScore5H8C v){
    return v.tested>=90&&v.old_tested>=12&&v.new_tested>=12&&
           (uint64_t)v.matched*100>= (uint64_t)v.tested*96&&
           (uint64_t)v.old_matched*100>=(uint64_t)v.old_tested*96&&
           (uint64_t)v.new_matched*100>=(uint64_t)v.new_tested*96;
}
unsigned cviii_dual_margins_5h8c(const CviiiRoomRenderSource *old_scene,
    unsigned ox,unsigned oy,unsigned ot,const CviiiRoomRenderSource *new_scene,
    unsigned nx,unsigned ny,unsigned nt,unsigned split,int vertical,
    uint32_t *dst,unsigned w,unsigned center){
    if(!dst||w!=426||center!=85)return 0;
    unsigned count=0;
    for(unsigned y=48;y<240;y++)for(unsigned x=0;x<w;x++){
        if(x>=center&&x<center+256)continue;
        const int native_x=(int)x-(int)center;
        const bool old=vertical==0?native_x<(int)split:vertical==1?y<split:vertical==2?native_x>=(int)split:y>=split;
        uint32_t c;
        if(scene(old?old_scene:new_scene,old?ox:nx,old?oy:ny,
                 old?ot:nt,native_x,y,&c)){
            dst[(size_t)y*w+x]=c;count++;
        }
    }
    return count;
}
