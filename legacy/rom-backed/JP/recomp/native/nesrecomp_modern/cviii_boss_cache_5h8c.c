#include "cviii_boss_cache_5h8c.h"
#include <string.h>
#include <stddef.h>
static unsigned idx(int x,int y){
    x%=512;if(x<0)x+=512;
    y%=480;if(y<0)y+=480;
    return (unsigned)y*512+(unsigned)x;
}
static int sprite(const uint8_t oam[256],unsigned x,unsigned y){
    if(!oam)return 0;
    for(unsigned i=0;i<64;i++){
        const uint8_t *p=oam+i*4;
        if(p[0]>=239)continue;
        if(x>=p[3]&&x<(unsigned)p[3]+8&&y>(unsigned)p[0]&&y<=(unsigned)p[0]+16)return 1;
    }
    return 0;
}
void cviii_boss_cache_clear_5h8c(CviiiBossCache5H8C *c){if(c)memset(c,0,sizeof *c);}
unsigned cviii_boss_cache_match_5h8c(const CviiiBossCache5H8C *c,
    const uint32_t native[256*240],const uint8_t oam[256],
    const int sx[240],const int sy[240],unsigned *matches){
    unsigned tested=0,matched=0;
    if(!c||!native||!sx||!sy){if(matches)*matches=0;return 0;}
    for(unsigned y=64;y<=224;y+=16)for(unsigned x=16;x<=240;x+=16){
        if(sprite(oam,x,y))continue;
        unsigned p=idx(sx[y]+(int)x,sy[y]);
        if(c->known[p]){tested++;matched+=(c->color[p]==native[y*256+x]);}
    }
    if(matches)*matches=matched;
    return tested;
}
void cviii_boss_cache_observe_5h8c(CviiiBossCache5H8C *c,uint64_t frame,
    const uint32_t native[256*240],const uint8_t oam[256],
    const int sx[240],const int sy[240]){
    if(!c||!frame||c->last_frame==frame||!native||!sx||!sy)return;
    /* OAM is rasterized once; avoid scanning 64 sprites for every pixel. */
    uint8_t covered[256*240]={0};
    if(oam)for(unsigned slot=0;slot<64;slot++){
        const uint8_t *a=oam+slot*4;
        if(a[0]>=239)continue;
        for(unsigned dy=1;dy<=16;dy++){
            unsigned y=(unsigned)a[0]+dy;
            if(y>=240)break;
            for(unsigned dx=0;dx<8;dx++){
                unsigned x=(unsigned)a[3]+dx;
                if(x<256)covered[y*256+x]=1;
            }
        }
    }
    for(unsigned y=48;y<240;y++)for(unsigned x=0;x<256;x++){
        if(covered[y*256+x])continue;
        unsigned p=idx(sx[y]+(int)x,sy[y]);
        const uint32_t color=native[(size_t)y*256+x];
        if(c->known[p] && c->color[p]==color){
            if(c->stable[p]<3)c->stable[p]++;
        }else c->stable[p]=1;
        c->color[p]=color;c->known[p]=1;
    }
    c->last_frame=frame;
}
unsigned cviii_boss_cache_paint_5h8c(const CviiiBossCache5H8C *c,
    uint32_t *dest,const int sx[240],const int sy[240]){
    if(!c||!dest||!sx||!sy)return 0;
    unsigned painted=0;
    for(unsigned y=48;y<240;y++)for(unsigned x=0;x<426;x++){
        if(x>=85&&x<341)continue;
        unsigned p=idx(sx[y]+(int)x-85,sy[y]);
        if(c->known[p]){dest[(size_t)y*426+x]=c->color[p];painted++;}
    }
    return painted;
}

/* Display qualification uses observations from at least two different
 * frames; an unstable pixel never becomes an offscreen extension. */
unsigned cviii_boss_cache_match_stable_5h8d(const CviiiBossCache5H8C *c,
    const uint32_t native[256*240],const uint8_t oam[256],
    const int sx[240],const int sy[240],unsigned *matches){
    unsigned tested=0,matched=0;
    if(!c||!native||!sx||!sy){if(matches)*matches=0;return 0;}
    for(unsigned y=64;y<=224;y+=16)for(unsigned x=16;x<=240;x+=16){
        if(sprite(oam,x,y))continue;
        unsigned p=idx(sx[y]+(int)x,sy[y]);
        if(c->known[p] && c->stable[p]>=2){
            tested++;matched+=(c->color[p]==native[(size_t)y*256+x]);
        }
    }
    if(matches)*matches=matched;
    return tested;
}
unsigned cviii_boss_cache_paint_stable_5h8d(const CviiiBossCache5H8C *c,
    uint32_t *dest,const int sx[240],const int sy[240]){
    if(!c||!dest||!sx||!sy)return 0;
    unsigned painted=0;
    for(unsigned y=48;y<240;y++)for(unsigned x=0;x<426;x++){
        if(x>=85&&x<341)continue;
        unsigned p=idx(sx[y]+(int)x-85,sy[y]);
        if(c->known[p] && c->stable[p]>=2){
            dest[(size_t)y*426+x]=c->color[p];painted++;
        }
    }
    return painted;
}
