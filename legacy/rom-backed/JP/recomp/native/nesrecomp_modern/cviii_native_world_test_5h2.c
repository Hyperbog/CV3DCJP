/* Synthetic, ROM-free test for index provenance, adjacency, and margin clipping. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_native_world_5h2.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static CviiiWholeRoomPack rooms;
static CviiiRoomGfx gfx;
static uint8_t cells[CVIII_WORLD_INDEX_ROOMS][144];
static uint16_t widths[CVIII_WORLD_INDEX_ROOMS];
static uint64_t fnv(const uint8_t *p,size_t n){
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;
}
static void wr16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void wr32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void wr64(uint8_t *p,uint64_t v){for(int i=0;i<8;i++)p[i]=(uint8_t)(v>>(8*i));}
bool cviii_whole_room_find(const CviiiWholeRoomPack *p,uint8_t s,uint8_t a,
                          uint8_t r,CviiiWholeRoomView *out) {
    if(!p||!p->allocation||r||s>=15||a>=9||!out)return false;
    *out=p->rooms[(unsigned)s*9u+a];return true;
}
const CviiiGfxGroup *cviii_room_gfx_group(const CviiiRoomGfx *g,const CviiiWholeRoomView *room) {
    if(!g||!g->allocation||!room||room->stage>=15)return NULL;
    return &g->groups[room->stage];
}
bool cviii_room_render_validate(const CviiiRoomRenderSource *src){
    return src && src->room && src->room->metatiles && src->graphics && src->chr;
}
bool cviii_room_render_view(const CviiiRoomRenderSource *src,unsigned cx,unsigned cy,
        uint32_t *dst,unsigned w,unsigned h,size_t stride) {
    if(!cviii_room_render_validate(src)||cx+w>(unsigned)src->room->width*32u||
       cy+h>(unsigned)src->room->height*32u||stride<w)return false;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)
        dst[(size_t)y*stride+x]=UINT32_C(0xff000000)|src->room->area;
    return true;
}
static void make_fixture(const char *path) {
    static const uint8_t sha[32]={
        0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
        0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28
    };
    uint8_t data[64+CVIII_WORLD_INDEX_ROOMS*32]={0};
    memcpy(data,"CVH2IDX\0",8);
    wr16(data+8,1);wr16(data+10,CVIII_WORLD_INDEX_ROOMS);
    wr32(data+12,sizeof data);memcpy(data+16,sha,32);
    rooms.allocation=(uint8_t*)cells;gfx.allocation=(uint8_t*)cells;
    for(unsigned i=0;i<CVIII_WORLD_INDEX_ROOMS;i++){
        unsigned stage=i/9u,area=i%9u;
        widths[i]=i==0?24u:8u;
        memset(cells[i],1,sizeof cells[i]);
        uint32_t roff=0x18010u+i*48u;
        rooms.rooms[i]=(CviiiWholeRoomView){0};
        rooms.rooms[i].stage=(uint8_t)stage;rooms.rooms[i].area=(uint8_t)area;
        rooms.rooms[i].width=widths[i];rooms.rooms[i].height=6u;
        rooms.rooms[i].source_rom_offset=roff;rooms.rooms[i].metatiles=cells[i];
        uint8_t *p=data+64+i*32;
        p[0]=(uint8_t)stage;p[1]=(uint8_t)area;p[3]=(uint8_t)stage;
        p[4]=i<2?1:0;
        wr32(p+8,i==1?768u:0u);wr32(p+12,0);
        wr16(p+16,widths[i]);wr16(p+18,6);
        wr32(p+20,roff);wr64(p+24,fnv(cells[i],widths[i]*6u));
    }
    wr64(data+48,fnv(data+64,sizeof data-64));
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(data,1,sizeof data,f)==sizeof data);assert(!fclose(f));
}
int main(void){
    const char *path="cviii_native_world_test_5h2.idx";
    make_fixture(path);
    CviiiNativeWorld5H2 *index=calloc(1,sizeof *index);assert(index);
    assert(cviii_world_index_open_5h2(index,path,&rooms,&gfx));
    const CviiiWorldEntry5H2 *a=cviii_world_index_find_5h2(index,0,0,0);
    const CviiiWorldEntry5H2 *b=cviii_world_index_find_5h2(index,0,1,0);
    assert(a&&b&&a->placed&&b->placed&&a->x==0&&b->x==768);
    assert(!cviii_world_index_find_5h2(index,0,9,0));
    assert(!cviii_world_index_find_5h2(index,14,8,1));
    CviiiChrIndex chr={0};
    CviiiRoomRenderSource src={0};src.graphics=&gfx;src.room=&rooms.rooms[0];src.chr=&chr;
    const unsigned W=426,H=240,X0=85;uint32_t *image=malloc(W*H*sizeof *image);assert(image);
    for(unsigned j=0;j<W*H;j++)image[j]=0xff334455u;
    uint32_t count=cviii_world_margins_5h2(index,&rooms,&src,720,0,48,image,W,H,X0);
    /* Camera at x=720; valid right neighbor [768,1024], no third room guessed. */
    assert(count>0&&image[100*W+0]==0xff000000u);
    assert(image[100*W+341]==0xff000001u);
    assert(image[100*W+425]==0xff334455u);
    assert(image[100*W+85]==0xff334455u); /* original game untouched */
    assert(image[20*W+0]==0xff334455u);/* protected HUD */
    FILE *f=fopen(path,"r+b");assert(f);assert(!fseek(f,112,SEEK_SET));
    int old=fgetc(f);assert(old!=EOF);assert(!fseek(f,112,SEEK_SET));
    assert(fputc(old^1,f)!=EOF);fclose(f);
    assert(!cviii_world_index_open_5h2(index,path,&rooms,&gfx));
    assert(index->loaded); /* failed load cannot destroy valid index */
    remove(path);free(image);free(index);
    puts("PASS 5H2 synthetic index, Stage 1 adjacency, unclipped margins, fail-closed authentication");
    return 0;
}
