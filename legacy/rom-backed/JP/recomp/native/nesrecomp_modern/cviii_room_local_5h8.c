#include "cviii_room_local_5h8.h"
#include <string.h>
#include <limits.h>
#include <stddef.h>

void cviii_local_reset_5h8(CviiiLocalTracker5H8 *t) { if(t)memset(t,0,sizeof *t); }
static unsigned add(unsigned a[8],unsigned n,int64_t v,unsigned length)
{
    if(v<0 || v>=length || n>=8)return n;
    for(unsigned i=0;i<n;i++)if(a[i]==(unsigned)v)return n;
    a[n++]=(unsigned)v;return n;
}
unsigned cviii_local_seeds_5h8(const CviiiLocalTracker5H8 *t,
    uint64_t frame,const CviiiWholeRoomView *room,int scroll,
    unsigned ram_hint,unsigned out[8])
{
    if(!room||!out||scroll<0||scroll>=(room->metadata&0x80u?480:512))return 0;
    const bool vertical=(room->metadata&0x80u)!=0;
    const unsigned length=(vertical?room->height:room->width)*32u;
    if(!length||length>4096)return 0;
    unsigned n=0;
    if(t && t->valid && t->stage==room->stage &&
       t->area==room->area && t->room==room->room &&
       frame>t->frame && frame<=t->frame+2) {
        const int period=vertical?480:512;
        int delta=(scroll-t->scroll+period)%period;
        if(delta>period/2)delta-=period;
        if(delta>=-32 && delta<=32)
            n=add(out,n,(int64_t)t->camera+delta,length);
    }
    /* Candidate pages stay within the selected room; no graph placement. */
    n=add(out,n,scroll,length);
    n=add(out,n,scroll%(vertical?240:256),length);
    n=add(out,n,(scroll%(vertical?240:256))+(vertical?240:256),length);
    n=add(out,n,(int64_t)scroll+(vertical?480:512),length);
    if (!vertical) n=add(out,n,(int64_t)(scroll%256)+512,length);
    n=add(out,n,ram_hint,length);
    n=add(out,n,0,length);
    return n;
}
void cviii_local_confirm_5h8(CviiiLocalTracker5H8 *t,
    uint64_t frame,const CviiiWholeRoomView *r,int scroll,unsigned camera)
{
    if(!t||!r)return;
    *t=(CviiiLocalTracker5H8){frame,r->stage,r->area,r->room,scroll,(int32_t)camera,true};
}
static bool actor_at(const uint8_t *oam,unsigned x,unsigned y)
{
    if(!oam)return false;
    for(unsigned i=0;i<64;i++) {
        const uint8_t *o=oam+i*4u;
        if(o[0]>=0xef)continue;
        unsigned sy=(unsigned)o[0]+1;
        if(x>=o[3] && x<(unsigned)o[3]+8 && y>=sy && y<sy+16)return true;
    }
    return false;
}
static bool pixel(const CviiiRoomRenderSource *s,int64_t x,int64_t y,uint32_t *out)
{
    if(!s||!s->room||!s->graphics||!s->chr||!out||x<0||y<0||
       x>=(int64_t)s->room->width*32 || y>=(int64_t)s->room->height*32)return false;
    CviiiGfxPixel p;
    return cviii_room_gfx_sample(s->graphics,s->room,s->chr,s->bg_bank_1k,
             (unsigned)x,(unsigned)y,&p) &&
           cviii_room_gfx_color(&p,s->palette_argb,out);
}
CviiiLocalScore5H8 cviii_local_match_5h8(const CviiiRoomRenderSource *s,
    unsigned cx,unsigned cy,unsigned top,const uint32_t *native,const uint8_t *oam)
{
    CviiiLocalScore5H8 v={0};
    if(!native||!s||top>=240)return v;
    for(unsigned y=64;y<=224;y+=16) {
        if(y<top)continue;
        for(unsigned x=16;x<=240;x+=16) {
            if(actor_at(oam,x,y))continue;
            uint32_t expected;
            if(!pixel(s,(int64_t)cx+x,(int64_t)cy+y-top,&expected))continue;
            v.tested++;
            v.matched+=(expected==native[(size_t)y*256+x]);
        }
    }
    return v;
}
unsigned cviii_local_margins_5h8(const CviiiRoomRenderSource *s,
    unsigned cx,unsigned cy,unsigned top,uint32_t *dst,
    unsigned width,unsigned height,unsigned center)
{
    if(!s||!dst||height!=240||width!=426||center!=85||top>=240)return 0;
    unsigned painted=0;
    for(unsigned y=48;y<height;y++) {
        if(y<top)continue;
        for(unsigned x=0;x<width;x++) {
            if(x>=center && x<center+256)continue;
            uint32_t color;
            if(pixel(s,(int64_t)cx+(int64_t)x-center,
                     (int64_t)cy+(int64_t)y-top,&color)) {
                dst[(size_t)y*width+x]=color;
                painted++;
            }
        }
    }
    return painted;
}
