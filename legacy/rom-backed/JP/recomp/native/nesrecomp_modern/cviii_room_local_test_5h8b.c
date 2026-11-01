/* ROM-free room-local fallback contract. Assert-based, including Release. */
#include "cviii_room_local_5h8.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

bool cviii_room_gfx_sample(const CviiiRoomGfx *g,const CviiiWholeRoomView *r,
                           const CviiiChrIndex *chr,const uint8_t banks[4],
                           unsigned x,unsigned y,CviiiGfxPixel *p)
{
    (void)g;(void)chr;(void)banks;
    if(!p || x>=r->width*32u || y>=r->height*32u)return false;
    memset(p,0,sizeof *p);
    p->pixel2=(uint8_t)(((x/16u)+3u*(y/16u))%4u);
    return true;
}
bool cviii_room_gfx_color(const CviiiGfxPixel *p,const uint32_t colors[16],uint32_t *out)
{
    if(!p||!colors||!out)return false;
    *out=colors[p->pixel2];return true;
}
static void exercise(void)
{
    CviiiWholeRoomView r={0};r.stage=0;r.area=1;r.room=1;
    r.width=16;r.height=30;r.metadata=0x80u;
    CviiiRoomGfx gfx={0};CviiiChrIndex chr={0};
    CviiiRoomRenderSource src={0};src.graphics=&gfx;src.room=&r;src.chr=&chr;
    for(int i=0;i<16;i++)src.palette_argb[i]=0xff000000u+(uint32_t)i*0x10204u;
    uint32_t *native=(uint32_t*)calloc(256u*240u,sizeof(uint32_t));
    uint32_t *wide=(uint32_t*)malloc(426u*240u*sizeof(uint32_t));
    assert(native&&wide);
    const unsigned camera_y=404,top=48;
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++){
        CviiiGfxPixel pixel={0};uint32_t color=0;
        assert(cviii_room_gfx_sample(&gfx,&r,&chr,src.bg_bank_1k,x,
                                  camera_y+y-top,&pixel));
        assert(cviii_room_gfx_color(&pixel,src.palette_argb,&color));
        native[y*256u+x]=color;
    }
    uint8_t oam[256];memset(oam,0xff,sizeof oam);
    CviiiLocalScore5H8 m=cviii_local_match_5h8(&src,0,camera_y,top,native,oam);
    assert(m.tested>=90&&m.matched==m.tested);
    m=cviii_local_match_5h8(&src,0,camera_y+16,top,native,oam);
    assert(m.tested>=90&&m.matched<m.tested*96u/100u);
    const uint32_t matte=0xff090909u;
    for(unsigned y=0;y<240;y++){
        for(unsigned x=0;x<426;x++)wide[y*426u+x]=matte;
        memcpy(wide+y*426u+85u,native+y*256u,256u*sizeof(uint32_t));
    }
    assert(cviii_local_margins_5h8(&src,0,camera_y,top,wide,426,240,85)>0);
    assert(wide[120u*426u+85u]==native[120u*256u]);
    assert(wide[120u*426u+0u]==matte); /* before the room's left edge */
    assert(wide[120u*426u+425u]!=matte); /* known room geometry on right */
    assert(wide[15u*426u+425u]==matte); /* never paint HUD margin */
    CviiiLocalTracker5H8 tracker={0};unsigned seeds[8]={0};
    cviii_local_confirm_5h8(&tracker,200,&r,478,478);
    unsigned n=cviii_local_seeds_5h8(&tracker,201,&r,2,0,seeds);
    assert(n>0&&seeds[0]==482u); /* vertical modulo 480, not 512 */
    r.metadata=0;r.width=24;r.height=6;
    cviii_local_confirm_5h8(&tracker,100,&r,510,510);
    n=cviii_local_seeds_5h8(&tracker,101,&r,2,0,seeds);
    assert(n>0&&seeds[0]==514u); /* horizontal modulo 512 */
    n=cviii_local_seeds_5h8(&tracker,110,&r,3,0,seeds);
    assert(n>0&&seeds[0]!=515u); /* discontinuity -> no tracker prediction */
    free(native);free(wide);
}
int main(void){exercise();return 0;}
