#include "cviii_scene_epoch_5h8c.h"
#include "cviii_dual_scene_5h8c.h"
#include "cviii_boss_cache_5h8c.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Synthetic renderer stubs: known room identity has a distinct pixel. */
bool cviii_room_gfx_sample(const CviiiRoomGfx *g,const CviiiWholeRoomView *r,
    const CviiiChrIndex *chr,const uint8_t banks[4],unsigned x,unsigned y,
    CviiiGfxPixel *p){
    (void)g;(void)chr;(void)banks;(void)x;(void)y;
    if(!r||!p)return false;
    memset(p,0,sizeof *p);p->pattern=r->room+1;return true;
}
bool cviii_room_gfx_color(const CviiiGfxPixel *p,const uint32_t palette[16],
                          uint32_t *c){(void)palette;if(!p||!c)return false;
                          *c=0xff000000u|(uint32_t)p->pattern;return true;}
int main(void){
    CviiiSceneEpoch5H8C epoch={0}; CviiiSceneEvent5H8C event={0};
    uint8_t ciram[2048]={0},pal[32]={0};
    assert(cviii_scene_epoch_observe_5h8c(&epoch,1,0,3,0,ciram,2048,pal,1,&event));
    for(int i=0;i<909;i++)ciram[i]=1;
    assert(cviii_scene_epoch_observe_5h8c(&epoch,2,0,3,0,ciram,2048,pal,1,&event));
    assert(epoch.epoch==2&&event.ciram_changed==909);
    CviiiWholeRoomView r0={0},r1={0};
    r0.room=0;r1.room=1;r0.width=r1.width=40;
    r0.height=r1.height=20;
    CviiiRoomGfx gfx={0};CviiiChrIndex chr={0};
    CviiiRoomRenderSource a={0},b={0};
    a.room=&r0;a.graphics=&gfx;a.chr=&chr;
    b.room=&r1;b.graphics=&gfx;b.chr=&chr;
    uint32_t *native=malloc(256*240*sizeof(uint32_t));
    uint32_t *wide=calloc(426*240,sizeof(uint32_t));
    CviiiBossCache5H8C *cache=calloc(1,sizeof *cache);
    assert(native&&wide&&cache);
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)
        native[y*256+x]=0xff000000u|(x<128?1u:2u);
    CviiiDualScore5H8C score=cviii_dual_score_5h8c(&a,256,0,32,&b,256,0,32,
                                         128,0,native,NULL);
    assert(cviii_dual_qualified_5h8c(score));
    assert(score.tested>=90&&score.old_tested>=12&&score.new_tested>=12);
    assert(cviii_dual_margins_5h8c(&a,256,0,32,&b,256,0,32,128,0,wide,426,85)>0);
    assert(wide[100*426+0]==0xff000001u&&wide[100*426+425]==0xff000002u);
    native[96*256+32]=0xff0000f0u;
    assert(!cviii_dual_qualified_5h8c((CviiiDualScore5H8C){90,0,45,0,45,0}));
    int sx[240]={0},sy[240]={0};
    for(int y=0;y<240;y++)sy[y]=y;
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)
        native[y*256+x]=0xff000000u|((x+y)&255u);
    cviii_boss_cache_observe_5h8c(cache,1,native,NULL,sx,sy);
    unsigned match=0;
    assert(cviii_boss_cache_match_5h8c(cache,native,NULL,sx,sy,&match)>=90);
    assert(cviii_boss_cache_paint_5h8c(cache,wide,sx,sy)==0);
    for(int y=0;y<240;y++)sx[y]=20;
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)
        native[y*256+x]=0xff000000u|((x+y+20u)&255u);
    unsigned tested=cviii_boss_cache_match_5h8c(cache,native,NULL,sx,sy,&match);
    assert(tested>=90&&match==tested);
    assert(cviii_boss_cache_paint_5h8c(cache,wide,sx,sy)>0);
    cviii_boss_cache_clear_5h8c(cache);
    assert(cviii_boss_cache_paint_5h8c(cache,wide,sx,sy)==0);
    free(cache);free(wide);free(native);
    puts("PASS: two-scene splits, strict gates, observed-only dynamic background, epoch resets");
    return 0;
}
