#include "cviii_transition_5h8d.h"
#include <string.h>
#include <stddef.h>
#include <limits.h>
void cviii_transition_reset_5h8d(CviiiTransitionTrack5H8D *t){
    if(t)memset(t,0,sizeof *t);
}
static int delta_wrap(int current,int old,int period){
    int d=(current-old+period)%period;
    if(d<0)d+=period;
    return d>period/2?d-period:d;
}
bool cviii_transition_step_5h8d(CviiiTransitionTrack5H8D *t,
    uint64_t verified,uint64_t frame,
    uint8_t stage,uint8_t area,uint8_t room,
    int initial_x,int initial_y,int old_sx,int old_sy,
    int sx,int sy,bool vertical,int64_t *out_x,int64_t *out_y){
    if(!t||!out_x||!out_y||!verified||frame<=verified||
       frame>verified+480||initial_x<0||initial_y<0)return false;
    if(!t->valid||t->start!=verified||t->stage!=stage||
       t->area!=area||t->room!=room){
        *t=(CviiiTransitionTrack5H8D){0};
        t->valid=true;t->start=verified;t->frame=verified;
        t->stage=stage;t->area=area;t->room=room;
        t->x=initial_x;t->y=initial_y;t->sx=old_sx;t->sy=old_sy;
    }
    if(frame<t->frame||frame>t->frame+2){t->valid=false;return false;}
    if(frame!=t->frame){
        int d=vertical?delta_wrap(sy,t->sy,480):delta_wrap(sx,t->sx,512);
        /* A discontinuous PPU setup is not evidence of camera motion. */
        if(d < -32 || d > 32){t->valid=false;return false;}
        if(vertical)t->y+=d;else t->x+=d;
        t->frame=frame;t->sx=sx;t->sy=sy;
    }
    if(t->x<0||t->y<0||t->x>INT_MAX||t->y>INT_MAX)return false;
    *out_x=t->x;*out_y=t->y;return true;
}
static bool actor(const uint8_t oam[256],unsigned x,unsigned y){
    if(!oam)return false;
    for(unsigned slot=0;slot<64;slot++){
        const uint8_t *p=oam+slot*4;
        if(p[0]>=239)continue;
        if(x>=p[3]&&x<(unsigned)p[3]+8&&y>(unsigned)p[0]&&y<=(unsigned)p[0]+16)
            return true;
    }
    return false;
}
static bool pixel(const CviiiRoomRenderSource *s,unsigned cx,unsigned cy,
                  unsigned top,unsigned x,unsigned y,uint32_t *out){
    if(!s||!s->room||!s->graphics||!s->chr||!out||y<top)return false;
    uint64_t xx=(uint64_t)cx+x,yy=(uint64_t)cy+y-top;
    if(xx>=(uint64_t)s->room->width*32||
       yy>=(uint64_t)s->room->height*32)return false;
    CviiiGfxPixel p;
    return cviii_room_gfx_sample(s->graphics,s->room,s->chr,s->bg_bank_1k,
         (unsigned)xx,(unsigned)yy,&p)&&cviii_room_gfx_color(&p,s->palette_argb,out);
}
CviiiTransitionPlan5H8D cviii_transition_best_5h8d(
    const CviiiRoomRenderSource *old_s,unsigned ox,unsigned oy,unsigned ot,
    const CviiiRoomRenderSource *new_s,unsigned nx,unsigned ny,unsigned nt,
    const uint32_t native[256*240],const uint8_t oam[256]){
    CviiiTransitionPlan5H8D best={0};
    if(!old_s||!new_s||!native)return best;
    /* Precompute pixel parity once; split search is then cheap. */
    uint8_t a_valid[11][15]={0},b_valid[11][15]={0};
    uint8_t a_hit[11][15]={0},b_hit[11][15]={0};
    for(unsigned iy=0;iy<11;iy++)for(unsigned ix=0;ix<15;ix++){
        unsigned y=64+iy*16,x=16+ix*16;
        if(actor(oam,x,y))continue;
        uint32_t c;
        a_valid[iy][ix]=pixel(old_s,ox,oy,ot,x,y,&c);
        a_hit[iy][ix]=a_valid[iy][ix]&&(c==native[y*256+x]);
        b_valid[iy][ix]=pixel(new_s,nx,ny,nt,x,y,&c);
        b_hit[iy][ix]=b_valid[iy][ix]&&(c==native[y*256+x]);
    }
    for(unsigned dir=0;dir<4;dir++)for(unsigned split=16;split<=224;split+=16){
        CviiiDualScore5H8C s={0};
        for(unsigned iy=0;iy<11;iy++)for(unsigned ix=0;ix<15;ix++){
            unsigned y=64+iy*16,x=16+ix*16;
            bool old=(dir==0?x<split:dir==1?y<split:
                      dir==2?x>=split:y>=split);
            unsigned v=old?a_valid[iy][ix]:b_valid[iy][ix];
            unsigned h=old?a_hit[iy][ix]:b_hit[iy][ix];
            if(!v)continue;
            s.tested++;s.matched+=h;
            if(old){s.old_tested++;s.old_matched+=h;}
            else{s.new_tested++;s.new_matched+=h;}
        }
        if(cviii_dual_qualified_5h8c(s)&&
           (s.tested>best.score.tested||
            (s.tested==best.score.tested&&s.matched>best.score.matched))){
            best=(CviiiTransitionPlan5H8D){s,split,dir};
        }
    }
    return best;
}

/* Paint only authenticated room pixels, never the native 256-pixel picture. */
static bool margin_pixel(const CviiiRoomRenderSource *src,
    unsigned cx,unsigned cy,unsigned top,int native_x,unsigned y,uint32_t *out){
    if(!src||!src->room||!src->graphics||!src->chr||!out||y<top)return false;
    int64_t xx=(int64_t)cx+native_x,yy=(int64_t)cy+y-(int64_t)top;
    if(xx<0||yy<0||xx>=(int64_t)src->room->width*32||
       yy>=(int64_t)src->room->height*32)return false;
    CviiiGfxPixel p;
    return cviii_room_gfx_sample(src->graphics,src->room,src->chr,
        src->bg_bank_1k,(unsigned)xx,(unsigned)yy,&p)&&
        cviii_room_gfx_color(&p,src->palette_argb,out);
}
unsigned cviii_transition_paint_5h8d(
    const CviiiRoomRenderSource *old_scene,unsigned ox,unsigned oy,unsigned ot,
    const CviiiRoomRenderSource *new_scene,unsigned nx,unsigned ny,unsigned nt,
    CviiiTransitionPlan5H8D plan,uint32_t *dst,unsigned width,unsigned center){
    if(!dst||width!=426||center!=85||plan.direction>3||
       plan.split<16||plan.split>224||
       !cviii_dual_qualified_5h8c(plan.score))return 0;
    unsigned painted=0;
    for(unsigned y=48;y<240;y++)for(unsigned x=0;x<width;x++){
        if(x>=center&&x<center+256)continue;
        int native_x=(int)x-(int)center;
        bool use_old=plan.direction==0?native_x<(int)plan.split:
            plan.direction==1?y<plan.split:
            plan.direction==2?native_x>=(int)plan.split:y>=plan.split;
        uint32_t color=0;
        if(margin_pixel(use_old?old_scene:new_scene,
            use_old?ox:nx,use_old?oy:ny,use_old?ot:nt,native_x,y,&color)){
            dst[(size_t)y*width+x]=color;
            painted++;
        }
    }
    return painted;
}
