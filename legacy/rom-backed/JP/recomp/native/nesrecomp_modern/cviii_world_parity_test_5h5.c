/* Synthetic-only world geometry; no retail ROM or game assets. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_world_parity_5h5.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static CviiiNativeWorld5H2 w;
static CviiiWholeRoomPack rooms;
static CviiiRoomGfx gfx;
static CviiiChrIndex chr;
static CviiiWholeRoomView first={0,0,0,0,0,0,0,0,0,24,6,0,NULL};
static CviiiWholeRoomView second={0,1,0,0,0,0,0,0,0,8,6,0,NULL};
static uint32_t native[256*240];
static uint8_t fake_layout=0;
const CviiiWorldEntry5H2 *cviii_world_index_find_5h2(
    const CviiiNativeWorld5H2 *v,uint8_t stage,uint8_t area,uint8_t room) {
    if(!v||!v->loaded||stage!=0||room!=0||area>1)return NULL;
    return &v->entries[area];
}
bool cviii_whole_room_find(const CviiiWholeRoomPack *p,uint8_t stage,
    uint8_t area,uint8_t room,CviiiWholeRoomView *out) {
    if(!p||!p->allocation||!out||stage||room||area>1)return false;
    *out=area?second:first;out->metatiles=&fake_layout;return true;
}
const CviiiGfxGroup *cviii_room_gfx_group(const CviiiRoomGfx *g,
    const CviiiWholeRoomView *room) {
    if(!g||!room||room->stage||room->room||room->area>1)return NULL;
    return &g->groups[0];
}
bool cviii_room_gfx_sample(const CviiiRoomGfx *g,const CviiiWholeRoomView *room,
    const CviiiChrIndex *index,const uint8_t bank[4],unsigned x,unsigned y,
    CviiiGfxPixel *out) {
    (void)index;(void)bank;
    if(!g||!room||!out||x>=(unsigned)room->width*32 ||
       y>=(unsigned)room->height*32)return false;
    out->pattern=0;
    /* Output encodes fine pixel position and room ID, not rounded tile index. */
    out->pixel2=(uint8_t)((x+room->area*7u)&3u);
    out->palette=(uint8_t)(room->area);
    return true;
}
bool cviii_room_gfx_color(const CviiiGfxPixel *px,
    const uint32_t argb[16],uint32_t *out){
    if(!px||!argb||!out)return false;
    *out=argb[px->palette*4u+px->pixel2];return true;
}
static void setup(void) {
    w.loaded=true;w.entries[0]=(CviiiWorldEntry5H2){0,0,0,0,1,0,0,24,6,0,0};
    w.entries[1]=(CviiiWorldEntry5H2){0,1,0,0,1,768,0,8,6,0,0};
    rooms.allocation=&fake_layout;
    gfx.groups[0].id=0;
}
static void frame(const CviiiRoomRenderSource *src,int cam,int top) {
    for(int y=0;y<240;y++)for(int x=0;x<256;x++) {
        uint32_t argb=0;uint8_t other=0;
        if(cviii_world_pixel_5h5(&w,&rooms,src,cam+x,y-top,
                                  &argb,&other))native[y*256+x]=argb;
        else native[y*256+x]=0xff112233u;
    }
}
int main(void) {
    setup();
    CviiiRoomRenderSource s={0};s.room=&second;s.graphics=&gfx;s.chr=&chr;
    for(unsigned i=0;i<16;i++)s.palette_argb[i]=0xff000000u+i*0x010101u;
    int32_t seam=0;
    assert(cviii_world_seam_candidate_5h5(&w,&second,0,&seam)&&seam==512);
    assert(cviii_world_seam_candidate_5h5(&w,&second,255,&seam)&&seam==767);
    assert(cviii_world_seam_candidate_5h5(&w,&second,256,&seam)&&seam==768);
    assert(!cviii_world_seam_candidate_5h5(&w,&first,0,&seam));
    assert(!cviii_world_seam_candidate_5h5(&w,&second,257,&seam));
    /* 700..955 crosses from 0/0/0 into 0/1/0; all samples agree. */
    frame(&s,700,48);
    CviiiWorldMatch5H5 m=cviii_world_match_5h5(&w,&rooms,&s,700,0,48,native,NULL);
    assert(m.tested>=90 && m.matched==m.tested && m.crossed>0);
    /* Advancing by ONE pixel must preserve parity, not snap to 8/16/32. */
    frame(&s,701,48);
    m=cviii_world_match_5h5(&w,&rooms,&s,701,0,48,native,NULL);
    assert(m.tested>=90 && m.matched==m.tested && m.crossed>0);
    CviiiWorldMatch5H5 wrong=cviii_world_match_5h5(&w,&rooms,&s,700,0,48,native,NULL);
    assert(wrong.matched<wrong.tested);
    frame(&s,512,48);
    m=cviii_world_match_5h5(&w,&rooms,&s,512,0,48,native,NULL);
    assert(m.tested>=90 && m.matched==m.tested);
    frame(&s,768,48);
    m=cviii_world_match_5h5(&w,&rooms,&s,768,0,48,native,NULL);
    assert(m.tested>=90 && m.matched==m.tested && m.crossed==0);
    /* No invented neighbors: an unplaced room yields no world pixels. */
    w.entries[0].placed=0;
    frame(&s,512,48);
    m=cviii_world_match_5h5(&w,&rooms,&s,512,0,48,native,NULL);
    assert(m.tested==0);
    puts("PASS: verified seam, cross-room samples, 1px scrolling, unplaced rejection");
    return 0;
}
