#include "cviii_native_world_5h2.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static uint16_t le16(const uint8_t *p) {return (uint16_t)(p[0]|(unsigned)p[1]<<8);}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint64_t le64(const uint8_t *p) {return (uint64_t)le32(p)|((uint64_t)le32(p+4)<<32);}
static int32_t signed32(const uint8_t *p) {
    uint32_t v=le32(p);
    return v <= INT32_MAX ? (int32_t)v : (int32_t)((int64_t)v-INT64_C(4294967296));
}
static uint64_t fnv64(const uint8_t *p,size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;
}
static const uint8_t rom_sha[32]={
    0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
    0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28
};
const CviiiWorldEntry5H2 *cviii_world_index_find_5h2(const CviiiNativeWorld5H2 *w,
    uint8_t s,uint8_t a,uint8_t r) {
    if(!w || !w->loaded)return NULL;
    unsigned key=((unsigned)s<<16)|((unsigned)a<<8)|r,lo=0,hi=CVIII_WORLD_INDEX_ROOMS;
    while(lo<hi){unsigned m=lo+(hi-lo)/2,have=((unsigned)w->entries[m].stage<<16)|
        ((unsigned)w->entries[m].area<<8)|w->entries[m].room;
        if(have<key)lo=m+1;else hi=m;}
    if(lo>=CVIII_WORLD_INDEX_ROOMS)return NULL;
    const CviiiWorldEntry5H2 *e=&w->entries[lo];
    return e->stage==s && e->area==a && e->room==r ? e : NULL;
}
bool cviii_world_index_open_5h2(CviiiNativeWorld5H2 *dst,const char *path,
    const CviiiWholeRoomPack *rooms,const CviiiRoomGfx *gfx) {
    if(!dst || !path || !rooms || !rooms->allocation || !gfx || !gfx->allocation)return false;
    uint8_t bytes[64+CVIII_WORLD_INDEX_ROOMS*32];
    FILE *file=fopen(path,"rb"); if(!file)return false;
    size_t n=fread(bytes,1,sizeof bytes,file);
    int extra=fgetc(file),error=ferror(file);fclose(file);
    if(n!=sizeof bytes||extra!=EOF||error||memcmp(bytes,"CVH2IDX\0",8) ||
       le16(bytes+8)!=1||le16(bytes+10)!=CVIII_WORLD_INDEX_ROOMS||
       le32(bytes+12)!=sizeof bytes||memcmp(bytes+16,rom_sha,32)||
       memcmp(bytes+56,"\0\0\0\0\0\0\0\0",8)||
       le64(bytes+48)!=fnv64(bytes+64,sizeof bytes-64))return false;
    CviiiNativeWorld5H2 candidate={0};
    unsigned prior=0,placements=0;
    for(unsigned i=0;i<CVIII_WORLD_INDEX_ROOMS;i++) {
        const uint8_t *p=bytes+64+i*32;
        uint8_t s=p[0],a=p[1],r=p[2],g=p[3],placed=p[4];
        uint32_t key=((uint32_t)s<<16)|((uint32_t)a<<8)|r;
        int32_t x=signed32(p+8),y=signed32(p+12);
        uint16_t w=le16(p+16),h=le16(p+18);
        uint32_t origin=le32(p+20);
        if((i && key<=prior)||s>=15||g>=15||placed>1||
           p[5]||p[6]||p[7])return false;
        prior=key;
        CviiiWholeRoomView room;
        if(!cviii_whole_room_find(rooms,s,a,r,&room)||
           room.width!=w||room.height!=h||room.source_rom_offset!=origin||
           le64(p+24)!=fnv64(room.metatiles,(size_t)w*h))return false;
        const CviiiGfxGroup *group=cviii_room_gfx_group(gfx,&room);
        if(!group||group!=&gfx->groups[g])return false;
        /* Only independently established JP0803 edge: no invented adjacency. */
        bool first=s==0&&a==0&&r==0,second=s==0&&a==1&&r==0;
        if(placed!=(first||second)|| (first&&(x!=0||y!=0||w!=24||h!=6)) ||
           (second&&(x!=768||y!=0||w!=8||h!=6)) ||
           (!placed&&(x||y)))return false;
        placements+=placed;
        candidate.entries[i]=(CviiiWorldEntry5H2){s,a,r,g,placed,x,y,w,h,origin,le64(p+24)};
    }
    if(placements!=2)return false;
    candidate.loaded=true;
    *dst=candidate;
    return true;
}
uint32_t cviii_world_margins_at_5h5(const CviiiNativeWorld5H2 *world,
    const CviiiWholeRoomPack *rooms,const CviiiRoomRenderSource *active,
    int32_t camera_world_x,int32_t camera_world_y,unsigned top,
    uint32_t *image,unsigned width,unsigned height,unsigned origin) {
    if(!world||!world->loaded||!rooms||!active||!active->room||!image||
       width<=256||width>512||height!=240||origin+256>width||top>=height)return 0;
    const CviiiWorldEntry5H2 *selected=cviii_world_index_find_5h2(world,
        active->room->stage,active->room->area,active->room->room);
    if(!selected||!selected->placed || !cviii_room_render_validate(active))return 0;
    const CviiiGfxGroup *current=cviii_room_gfx_group(active->graphics,active->room);
    const int64_t cam_x=camera_world_x;
    const int64_t cam_y=camera_world_y;
    uint32_t painted=0;
    for(unsigned i=0;i<CVIII_WORLD_INDEX_ROOMS;i++) {
        const CviiiWorldEntry5H2 *e=&world->entries[i];
        if(!e->placed||e->group!=selected->group)continue;
        CviiiWholeRoomView room;
        if(!cviii_whole_room_find(rooms,e->stage,e->area,e->room,&room))continue;
        if(cviii_room_gfx_group(active->graphics,&room)!=current)continue;
        const int64_t world_h=(int64_t)e->height*32,world_w=(int64_t)e->width*32;
        int64_t start_y=cam_y-e->y, end_y=start_y+(int64_t)(height-top);
        int64_t y0=start_y<0?-start_y:0, y1=end_y>world_h?world_h-start_y:(int64_t)(height-top);
        if(y0<0)y0=0;
        if(y1>(int64_t)(height-top))y1=height-top;
        if(y1<=y0)continue;
        CviiiRoomRenderSource source=*active;source.room=&room;
        unsigned ranges[2][2]={{0,origin},{origin+256,width}};
        for(unsigned edge=0;edge<2;edge++) {
            int64_t x0=ranges[edge][0],x1=ranges[edge][1];
            int64_t begin=cam_x-e->x+x0-(int64_t)origin;
            int64_t end=cam_x-e->x+x1-(int64_t)origin;
            if(begin<0)x0-=begin;
            if(end>world_w)x1-=end-world_w;
            if(x1<=x0)continue;
            uint64_t room_x=(uint64_t)(cam_x-e->x+x0-(int64_t)origin);
            uint64_t room_y=(uint64_t)(start_y+y0);
            if(room_x>UINT32_MAX||room_y>UINT32_MAX)continue;
            unsigned draw_w=(unsigned)(x1-x0),draw_h=(unsigned)(y1-y0);
            uint32_t *dst=image+(size_t)(top+y0)*width+(unsigned)x0;
            if(cviii_room_render_view(&source,(unsigned)room_x,(unsigned)room_y,
                                      dst,draw_w,draw_h,width))painted+=draw_w*draw_h;
        }
    }
    return painted;
}

uint32_t cviii_world_margins_5h2(const CviiiNativeWorld5H2 *w,
    const CviiiWholeRoomPack *rooms,const CviiiRoomRenderSource *src,
    unsigned camera_x,unsigned camera_y,unsigned top,
    uint32_t *image,unsigned width,unsigned height,unsigned origin)
{
    if(!w||!src||!src->room)return 0;
    const CviiiWorldEntry5H2 *e=cviii_world_index_find_5h2(w,
        src->room->stage,src->room->area,src->room->room);
    if(!e || !e->placed || (int64_t)e->x+camera_x>INT32_MAX ||
       (int64_t)e->y+camera_y>INT32_MAX)return 0;
    return cviii_world_margins_at_5h5(w,rooms,src,
        (int32_t)((int64_t)e->x+camera_x),
        (int32_t)((int64_t)e->y+camera_y),top,
        image,width,height,origin);
}
