#include "cviii_transition_5h8d.h"
#include "cviii_boss_cache_5h8c.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
bool cviii_room_gfx_sample(const CviiiRoomGfx *g,const CviiiWholeRoomView *r,
    const CviiiChrIndex *ch,const uint8_t banks[4],unsigned x,unsigned y,CviiiGfxPixel *p){
    (void)g;(void)ch;(void)banks;(void)x;(void)y;
    if(!r||!p)return false;
    memset(p,0,sizeof *p);p->pattern=(uint8_t)(r->room+1);return true;
}
bool cviii_room_gfx_color(const CviiiGfxPixel *p,const uint32_t colors[16],uint32_t *out){
    (void)colors;if(!p||!out)return false;*out=0xff000000u|p->pattern;return true;
}
int main(void){
    CviiiTransitionTrack5H8D t={0};int64_t x=0,y=0;
    assert(cviii_transition_step_5h8d(&t,100,101,0,2,0,250,0,510,120,511,120,false,&x,&y)&&x==251);
    assert(cviii_transition_step_5h8d(&t,100,102,0,2,0,250,0,510,120,0,120,false,&x,&y)&&x==252);
    assert(cviii_transition_step_5h8d(&t,100,103,0,2,0,250,0,510,120,1,120,false,&x,&y)&&x==253);
    assert(!cviii_transition_step_5h8d(&t,100,107,0,2,0,250,0,510,120,5,120,false,&x,&y));
    cviii_transition_reset_5h8d(&t);
    assert(cviii_transition_step_5h8d(&t,200,201,0,1,1,0,400,0,478,0,479,true,&x,&y)&&y==401);
    assert(cviii_transition_step_5h8d(&t,200,202,0,1,1,0,400,0,478,0,0,true,&x,&y)&&y==402);
    CviiiWholeRoomView ra={0},rb={0};ra.room=0;rb.room=1;
    ra.width=rb.width=40;ra.height=rb.height=20;
    CviiiRoomGfx gfx={0};CviiiChrIndex chr={0};
    CviiiRoomRenderSource a={0},b={0};
    a.room=&ra;a.graphics=&gfx;a.chr=&chr;
    b.room=&rb;b.graphics=&gfx;b.chr=&chr;
    uint32_t *native=malloc(256*240*sizeof *native);
    uint32_t *wide=calloc(426*240,sizeof *wide);
    CviiiBossCache5H8C *cache=calloc(1,sizeof *cache);
    assert(native&&wide&&cache);
    for(unsigned d=0;d<4;d++){
        for(unsigned sy=0;sy<240;sy++)for(unsigned sx=0;sx<256;sx++){
            bool old=d==0?sx<128:d==1?sy<128:d==2?sx>=128:sy>=128;
            native[sy*256+sx]=0xff000000u|(old?1u:2u);
        }
        CviiiTransitionPlan5H8D p=cviii_transition_best_5h8d(&a,256,0,32,
                                      &b,256,0,32,native,NULL);
        assert(p.score.tested>=90 && p.score.matched==p.score.tested);
        assert(p.direction==d);
        /* Verify PAINT direction, not just matching; preserve center pixels. */
        for(unsigned z=0;z<426*240;z++)wide[z]=0xff060606u;
        assert(cviii_transition_paint_5h8d(&a,256,0,32,&b,256,0,32,
                                           p,wide,426,85)>0);
        const uint32_t old_color=0xff000001u,new_color=0xff000002u;
        const uint32_t left_top=wide[64u*426u+8u];
        const uint32_t left_bottom=wide[176u*426u+8u];
        const uint32_t right_top=wide[64u*426u+409u];
        const uint32_t right_bottom=wide[176u*426u+409u];
        if(d==0){assert(left_top==old_color&&right_top==new_color);}
        if(d==1){assert(left_top==old_color&&left_bottom==new_color);}
        if(d==2){assert(left_top==new_color&&right_top==old_color);}
        if(d==3){assert(left_top==new_color&&left_bottom==old_color);}
        assert(right_bottom==((d==2||d==3)?old_color:new_color));
        assert(wide[64u*426u+85u]==0xff060606u);
        assert(wide[176u*426u+340u]==0xff060606u);
    }
    int sx[240]={0},sy[240]={0};
    for(int k=0;k<240;k++)sy[k]=k;
    for(unsigned yy=0;yy<240;yy++)for(unsigned xx=0;xx<256;xx++)
        native[yy*256+xx]=0xff000000u|((yy+xx)&255u);
    unsigned matched=0;
    cviii_boss_cache_observe_5h8c(cache,1,native,NULL,sx,sy);
    assert(cviii_boss_cache_match_stable_5h8d(cache,native,NULL,sx,sy,&matched)==0);
    cviii_boss_cache_observe_5h8c(cache,2,native,NULL,sx,sy);
    unsigned tested=cviii_boss_cache_match_stable_5h8d(cache,native,NULL,sx,sy,&matched);
    assert(tested>=90&&matched==tested);
    for(int k=0;k<240;k++)sx[k]=20;
    assert(cviii_boss_cache_paint_stable_5h8d(cache,wide,sx,sy)>0);
    cviii_boss_cache_clear_5h8c(cache);
    assert(cviii_boss_cache_paint_stable_5h8d(cache,wide,sx,sy)==0);
    free(cache);free(wide);free(native);
    puts("PASS 5H8D: persistent camera, both wrap periods, four transitions, strict scene cache");
    return 0;
}
