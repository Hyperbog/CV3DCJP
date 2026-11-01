#include "cviii_mutable_tiles_5h8e.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static CviiiMutableTiles5H8E ledger;
static uint32_t frame[256*240],wide[426*240];
static bool expected(void *ctx,unsigned x,unsigned y,uint32_t *out){
    (void)ctx;if(x>=768||y>=240)return false;
    *out=0xff000000u;return true;
}
static void block(unsigned wx,unsigned wy,uint32_t color){
    for(unsigned y=wy;y<wy+8;y++)for(unsigned x=wx;x<wx+8;x++)
        frame[y*256+x]=color;
}
int main(void){
    cviii_mut_visit_5h8e(&ledger,0,2,0);
    for(unsigned k=1;k<=3;k++){
        memset(frame,0,sizeof frame);
        for(unsigned i=0;i<256*240;i++)frame[i]=0xff000000u;
        block(240,96,0xff111111u);
        cviii_mut_observe_5h8e(&ledger,k,frame,NULL,0,0,48,expected,NULL);
    }
    /* Room coordinate (240,48) corresponds to screen (240,96). */
    assert(cviii_mut_count_5h8e(&ledger)==1);
    for(unsigned i=0;i<426*240;i++)wide[i]=0xff000000u;
    unsigned painted=cviii_mut_paint_5h8e(&ledger,wide,426,240,85,160,0,48);
    assert(painted==0);
    assert(wide[96*426+165]==0xff000000u); /* 85+240-160=165 */
    assert(wide[96*426+175]==0xff000000u);
    /* At the actual margin, move the camera sufficiently far right. */
    for(unsigned i=0;i<426*240;i++)wide[i]=0xff000000u;
    painted=cviii_mut_paint_5h8e(&ledger,wide,426,240,85,320,0,48);
    assert(painted==64);
    assert(wide[96*426+5]==0xff111111u);
    assert(wide[96*426+85]==0xff000000u); /* native center unchanged */
    cviii_mut_visit_5h8e(&ledger,0,2,1);
    assert(cviii_mut_count_5h8e(&ledger)==0);
    puts("PASS: repeated mutation, margin-only paint, room-visit reset");
    return 0;
}
