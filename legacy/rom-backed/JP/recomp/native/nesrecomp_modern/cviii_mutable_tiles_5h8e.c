#include "cviii_mutable_tiles_5h8e.h"
#include <string.h>
#include <stddef.h>

void cviii_mut_reset_5h8e(CviiiMutableTiles5H8E *s){if(s)memset(s,0,sizeof *s);}
void cviii_mut_visit_5h8e(CviiiMutableTiles5H8E *s,
                          uint8_t stage,uint8_t area,uint8_t room){
    if(!s)return;
    if(!s->active||s->stage!=stage||s->area!=area||s->room!=room){
        cviii_mut_reset_5h8e(s);s->active=1;
        s->stage=stage;s->area=area;s->room=room;
    }
}
static int sprite_tile(const uint8_t *oam,unsigned sx,unsigned sy){
    if(!oam)return 0;
    for(unsigned n=0;n<64;n++){
        const uint8_t *p=oam+4*n;
        if(p[0]>=239)continue;
        unsigned x=p[3],y=(unsigned)p[0]+1u;
        if(sx<x+8u&&sx+8u>x&&sy<y+16u&&sy+8u>y)return 1;
    }
    return 0;
}
static CviiiMutTile5H8E *slot(CviiiMutableTiles5H8E *s,
                               unsigned wx,unsigned wy,bool create){
    for(unsigned i=0;i<CVIII_MUT_SLOTS;i++)
        if(s->tiles[i].valid && s->tiles[i].x==wx && s->tiles[i].y==wy)
            return &s->tiles[i];
    if(!create)return NULL;
    for(unsigned i=0;i<CVIII_MUT_SLOTS;i++)if(!s->tiles[i].valid){
        CviiiMutTile5H8E *t=&s->tiles[i];
        memset(t,0,sizeof *t);t->x=(uint16_t)wx;t->y=(uint16_t)wy;
        t->valid=1;return t;
    }
    return NULL;
}
typedef struct{uint16_t x,y;uint32_t p[64];} Candidate;
void cviii_mut_observe_5h8e(CviiiMutableTiles5H8E *s,uint64_t frame,
    const uint32_t native[256*240],const uint8_t *oam,
    unsigned cx,unsigned cy,unsigned top,CviiiStaticPixel5H8E expected,void *ctx){
    if(!s||!s->active||!frame||frame<=s->last_frame||!native||!expected||
       top>=240||cx>4096||cy>4096)return;
    s->last_frame=frame;
    Candidate changed[8];unsigned found=0;
    /* The 8x8 boundaries are WORLD-aligned, not screen-aligned. */
    for(unsigned sy=top;sy+8<=240;sy++){
        if((cy+sy-top)%8u)continue;
        unsigned wy=cy+sy-top;
        for(unsigned sx=0;sx+8<=256;sx++){
            if((cx+sx)%8u)continue;
            unsigned wx=cx+sx;
            if(sprite_tile(oam,sx,sy))continue;
            unsigned diff=0;bool ok=true;
            uint32_t sample[64];
            for(unsigned dy=0;dy<8&&ok;dy++)for(unsigned dx=0;dx<8;dx++){
                uint32_t ref;
                if(!expected(ctx,wx+dx,wy+dy,&ref)){ok=false;break;}
                unsigned k=dy*8+dx;
                sample[k]=native[(size_t)(sy+dy)*256u+sx+dx];
                diff+=(sample[k]!=ref);
            }
            if(!ok)continue;
            CviiiMutTile5H8E *t=slot(s,wx,wy,false);
            if(diff<8){
                if(t&&t->confirmed){
                    if(++t->unchanged>=3)memset(t,0,sizeof *t);
                }else if(t)memset(t,0,sizeof *t);
                continue;
            }
            if(found==8){s->rejected_batches++;return;}
            changed[found].x=(uint16_t)wx;changed[found].y=(uint16_t)wy;
            memcpy(changed[found++].p,sample,sizeof sample);
        }
    }
    for(unsigned i=0;i<found;i++){
        Candidate *c=&changed[i];
        CviiiMutTile5H8E *t=slot(s,c->x,c->y,true);
        if(!t)continue;
        t->unchanged=0;
        if(t->observed_frame<frame && t->hits &&
           !memcmp(t->candidate,c->p,sizeof t->candidate)){
            if(t->hits<4)t->hits++;
        }else{
            memcpy(t->candidate,c->p,sizeof t->candidate);t->hits=1;
        }
        t->observed_frame=frame;
        if(t->hits>=3){
            memcpy(t->pixels,c->p,sizeof t->pixels);
            if(!t->confirmed){t->confirmed=1;s->committed++;}
        }
    }
}
unsigned cviii_mut_paint_5h8e(const CviiiMutableTiles5H8E *s,uint32_t *canvas,
    unsigned width,unsigned height,unsigned origin,
    unsigned cx,unsigned cy,unsigned top){
    if(!s||!s->active||!canvas||width<256||origin+256>width||height!=240)return 0;
    unsigned painted=0;
    for(unsigned i=0;i<CVIII_MUT_SLOTS;i++){
        const CviiiMutTile5H8E *t=&s->tiles[i];
        if(!t->valid||!t->confirmed)continue;
        int64_t px=(int64_t)origin+(int64_t)t->x-cx;
        int64_t py=(int64_t)top+(int64_t)t->y-cy;
        for(unsigned dy=0;dy<8;dy++)for(unsigned dx=0;dx<8;dx++){
            int64_t x=px+dx,y=py+dy;
            if(x<0||x>=(int64_t)width||y<0||y>=(int64_t)height)continue;
            if(x>=(int64_t)origin&&x<(int64_t)(origin+256))continue;
            canvas[(size_t)y*width+(size_t)x]=t->pixels[dy*8+dx];painted++;
        }
    }
    return painted;
}
unsigned cviii_mut_count_5h8e(const CviiiMutableTiles5H8E *s){
    unsigned n=0;if(!s)return 0;
    for(unsigned i=0;i<CVIII_MUT_SLOTS;i++)n+=s->tiles[i].valid&&s->tiles[i].confirmed;
    return n;
}
