#include "cviii_room_gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER 64u
#define GROUP_STRIDE 24u
#define ROOM_STRIDE 8u
#define PAYLOAD (HEADER+CVIII_GFX_GROUPS*GROUP_STRIDE+CVIII_GFX_ROOMS*ROOM_STRIDE)
static const uint8_t rom_sha[32]={
    0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,
    0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
    0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,
    0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28
};
static const uint8_t banks[CVIII_GFX_GROUPS]={8,8,8,8,8,7,7,7,7,6,6,7,6,6,6};
static const uint16_t counts[CVIII_GFX_GROUPS]={145,85,82,100,72,86,59,96,96,80,256,93,96,93,124};
static uint16_t rd16(const uint8_t *p) {return (uint16_t)(p[0]|(unsigned)p[1]<<8);}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint64_t rd64(const uint8_t *p) {return (uint64_t)rd32(p)|((uint64_t)rd32(p+4)<<32);}
static uint64_t fnv64(const uint8_t *p,size_t size) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<size;i++) {h^=p[i];h*=UINT64_C(1099511628211);}
    return h;
}
static uint8_t expected_group(uint8_t stage,uint8_t area,uint8_t room) {
    if(stage==13 && area==0 && room<2)return 2;
    if(stage==13 && ((area==2 && room==2)||(area==3 && room==0)))return 5;
    if(stage==14 && area==0 && room==1)return 1;
    return stage;
}
bool cviii_room_gfx_decode(const uint8_t *data,size_t size,
                           CviiiGfxGroup groups[CVIII_GFX_GROUPS],
                           CviiiGfxRoom rooms[CVIII_GFX_ROOMS])
{
    if(!data||!groups||!rooms||size<PAYLOAD||size>65536||
       memcmp(data,"CVGFX01\0",8)||rd16(data+8)!=1||
       rd16(data+10)!=CVIII_GFX_GROUPS||rd16(data+12)!=CVIII_GFX_ROOMS||
       rd16(data+14)!=0||rd32(data+16)!=size||rd32(data+20)!=PAYLOAD||
       memcmp(data+24,rom_sha,sizeof rom_sha)||
       rd64(data+56)!=fnv64(data+HEADER,size-HEADER))return false;
    CviiiGfxGroup g[CVIII_GFX_GROUPS];
    CviiiGfxRoom r[CVIII_GFX_ROOMS];
    size_t cursor=PAYLOAD;
    for(unsigned i=0;i<CVIII_GFX_GROUPS;i++) {
        const uint8_t *e=data+HEADER+i*GROUP_STRIDE;
        unsigned count=rd16(e+2),tile=rd32(e+12),attr=rd32(e+16);
        uint32_t orig_tile=rd32(e+4),orig_attr=rd32(e+8);
        if(e[0]!=i||e[1]!=banks[i]||count!=counts[i]||rd32(e+20)!=0||
           orig_tile<0x18010u||orig_tile>=0x24010u||
           orig_attr!=orig_tile+count*16u||
           tile!=cursor||attr!=tile+count*16u||
           attr>size||count>size-attr)return false;
        g[i]=(CviiiGfxGroup){(uint8_t)i,e[1],(uint16_t)count,
                             orig_tile,orig_attr,data+tile,data+attr};
        cursor=attr+count;
    }
    if(cursor!=size)return false;
    uint32_t last_key=0;
    for(unsigned i=0;i<CVIII_GFX_ROOMS;i++) {
        const uint8_t *e=data+HEADER+CVIII_GFX_GROUPS*GROUP_STRIDE+i*ROOM_STRIDE;
        unsigned stage=e[0],area=e[1],room=e[2],group=e[3];
        uint32_t key=(stage<<16)|(area<<8)|room,source=rd32(e+4);
        if(stage>=CVIII_GFX_GROUPS||group!=expected_group(stage,area,room)||
           (i && key<=last_key)||source<0x18010||source>=0x24010)return false;
        last_key=key;
        r[i]=(CviiiGfxRoom){(uint8_t)stage,(uint8_t)area,
             (uint8_t)room,(uint8_t)group,source};
    }
    memcpy(groups,g,sizeof g);
    memcpy(rooms,r,sizeof r);
    return true;
}
void cviii_room_gfx_close(CviiiRoomGfx *gfx) {
    if(!gfx)return;
    free(gfx->allocation);
    memset(gfx,0,sizeof *gfx);
}
bool cviii_room_gfx_open(CviiiRoomGfx *gfx,const char *filename) {
    if(!gfx||!filename||!*filename)return false;
    FILE *f=fopen(filename,"rb");
    if(!f)return false;
    uint8_t head[HEADER];
    if(fread(head,1,sizeof head,f)!=sizeof head||memcmp(head,"CVGFX01\0",8)) {
        fclose(f);return false;
    }
    uint32_t length=rd32(head+16);
    if(length<PAYLOAD||length>65536u) {fclose(f);return false;}
    uint8_t *data=(uint8_t *)malloc(length);
    if(!data) {fclose(f);return false;}
    memcpy(data,head,HEADER);
    size_t got=fread(data+HEADER,1,length-HEADER,f);
    int extra=fgetc(f),io=ferror(f);
    fclose(f);
    CviiiRoomGfx *next=(CviiiRoomGfx *)calloc(1,sizeof *next);
    if(!next) {free(data);return false;}
    bool ok=got==length-HEADER&&extra==EOF&&!io&&
            cviii_room_gfx_decode(data,length,next->groups,next->rooms);
    if(!ok) {free(data);free(next);return false;}
    next->allocation=data;
    next->size=length;
    cviii_room_gfx_close(gfx);
    *gfx=*next;
    free(next);
    return true;
}
const CviiiGfxGroup *cviii_room_gfx_group(const CviiiRoomGfx *gfx,
                                         const CviiiWholeRoomView *room)
{
    if(!gfx||!gfx->allocation||!room)return NULL;
    uint32_t key=((uint32_t)room->stage<<16)|((uint32_t)room->area<<8)|room->room;
    size_t lo=0,hi=CVIII_GFX_ROOMS;
    while(lo<hi) {
        size_t m=lo+(hi-lo)/2;
        const CviiiGfxRoom *entry=&gfx->rooms[m];
        uint32_t current=((uint32_t)entry->stage<<16)|((uint32_t)entry->area<<8)|entry->room;
        if(current<key)lo=m+1;else hi=m;
    }
    if(lo==CVIII_GFX_ROOMS)return NULL;
    const CviiiGfxRoom *entry=&gfx->rooms[lo];
    if(entry->stage!=room->stage||entry->area!=room->area||
       entry->room!=room->room||entry->layout_rom_offset!=room->source_rom_offset)
        return NULL;
    return &gfx->groups[entry->tileset_group];
}
bool cviii_room_gfx_sample(const CviiiRoomGfx *gfx,
                           const CviiiWholeRoomView *room,
                           const CviiiChrIndex *chr,
                           const uint8_t bg_bank_1k[4],
                           unsigned x,unsigned y,CviiiGfxPixel *out)
{
    if(!gfx||!room||!room->metatiles||!chr||!chr->pixels||
       !bg_bank_1k||!out||x>=(unsigned)room->width*32u||
       y>=(unsigned)room->height*32u)return false;
    const CviiiGfxGroup *g=cviii_room_gfx_group(gfx,room);
    if(!g)return false;
    unsigned mx=x/32u,my=y/32u;
    unsigned id=room->metatiles[my*room->width+mx];
    if(id>=g->count)return false;
    unsigned tx=(x%32u)/8u,ty=(y%32u)/8u;
    uint8_t pattern=g->patterns[id*16u+ty*4u+tx];
    uint8_t attr=g->attributes[id];
    unsigned quadrant=(ty/2u)*2u+tx/2u;
    uint8_t pal=(uint8_t)((attr>>(2u*quadrant))&3u);
    uint8_t bank=bg_bank_1k[pattern>>6];
    if(bank>=128)return false;
    unsigned physical=(unsigned)bank*64u+(pattern&63u);
    uint8_t pixel=chr->pixels[physical*64u+(y&7u)*8u+(x&7u)];
    if(pixel>3)return false;
    *out=(CviiiGfxPixel){pattern,pal,pixel,(uint16_t)physical};
    return true;
}
bool cviii_room_gfx_color(const CviiiGfxPixel *pixel,
                          const uint32_t bg_argb[16],uint32_t *color)
{
    if(!pixel||!bg_argb||!color||pixel->palette>3||pixel->pixel2>3)return false;
    unsigned index=pixel->pixel2?(unsigned)pixel->palette*4u+pixel->pixel2:0u;
    *color=bg_argb[index];
    return true;
}