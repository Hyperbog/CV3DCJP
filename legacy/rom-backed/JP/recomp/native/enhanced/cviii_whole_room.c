/* Versioned owner-local Japanese VRC6 whole-room asset pack.
 * The interface performs no CPU/PPU/mapper reads or writes. */
#include "cviii_whole_room.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t jp_sha256[32] = {
    0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,
    0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
    0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,
    0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28
};
static uint16_t u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (unsigned)p[1] << 8);
}
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 |
           (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint64_t u64(const uint8_t *p) {
    return (uint64_t)u32(p) | (uint64_t)u32(p+4)<<32;
}
static uint64_t fnv64(const uint8_t *p, size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++) { h ^= p[i]; h *= UINT64_C(1099511628211); }
    return h;
}

bool cviii_whole_room_decode(const uint8_t *data, size_t len,
                            CviiiWholeRoomView out[CVIII_WHOLE_ROOM_COUNT])
{
    if(!data || !out || len < CVIII_WHOLE_ROOM_HEADER +
            CVIII_WHOLE_ROOM_COUNT * CVIII_WHOLE_ROOM_STRIDE || len > 131072u ||
       memcmp(data,"CVROOM1\0",8) || u16(data+8)!=1 ||
       u16(data+10)!=CVIII_WHOLE_ROOM_COUNT || u32(data+12)!=len ||
       u32(data+16)!=CVIII_WHOLE_ROOM_HEADER+
                         CVIII_WHOLE_ROOM_COUNT*CVIII_WHOLE_ROOM_STRIDE ||
       u32(data+20)!=len-u32(data+16) ||
       memcmp(data+24,jp_sha256,sizeof jp_sha256) ||
       u64(data+56)!=fnv64(data+CVIII_WHOLE_ROOM_HEADER,
                              len-CVIII_WHOLE_ROOM_HEADER))
        return false;
    CviiiWholeRoomView scratch[CVIII_WHOLE_ROOM_COUNT];
    const size_t payload_begin=u32(data+16);
    uint32_t last_key=0;
    unsigned unique=0;
    for(unsigned i=0;i<CVIII_WHOLE_ROOM_COUNT;i++) {
        const uint8_t *p=data+CVIII_WHOLE_ROOM_HEADER+
                         i*CVIII_WHOLE_ROOM_STRIDE;
        const unsigned stage=p[0],area=p[1],room=p[2],meta=p[3];
        const unsigned screens=p[4],w=u16(p+10),h=u16(p+12);
        const unsigned count=u16(p+14);
        const uint32_t src=u32(p+16),off=u32(p+20);
        const uint32_t key=(stage<<16)|(area<<8)|room;
        const bool vertical=(meta&0x80u)!=0;
        if(stage>=15 || screens==0 || screens>16 ||
           (vertical ? (w!=8 || h!=screens*8) : (w!=screens*8 || h!=6)) ||
           count!=w*h || src<0x18010u || src>=0x24010u ||
           off>len || off<payload_begin || count>len-off ||
           p[5]>=128 || p[6]>=128 || p[7]>=128 || p[8]>=128 || p[9]!=0 ||
           (i && key<=last_key)) return false;
        for(unsigned j=24;j<CVIII_WHOLE_ROOM_STRIDE;j++)
            if(p[j]!=0) return false;
        last_key=key;
        /* Layout aliases must share the same grid and physical provenance. */
        bool previously_seen=false;
        for(unsigned j=0;j<i;j++) {
            if(scratch[j].source_rom_offset!=src) continue;
            previously_seen=true;
            if(scratch[j].metatiles!=data+off ||
               scratch[j].width!=w || scratch[j].height!=h ||
               scratch[j].screen_count!=screens ||
               !!(scratch[j].metadata&0x80u)!=vertical) return false;
            break;
        }
        if(!previously_seen) unique++;
        scratch[i]=(CviiiWholeRoomView){
            (uint8_t)stage,(uint8_t)area,(uint8_t)room,(uint8_t)meta,
            (uint8_t)screens,p[5],p[6],p[7],p[8],
            (uint16_t)w,(uint16_t)h,src,data+off
        };
    }
    if(unique!=130) return false;
    memcpy(out,scratch,sizeof scratch);
    return true;
}

void cviii_whole_room_close(CviiiWholeRoomPack *pack)
{
    if(!pack) return;
    free(pack->allocation);
    memset(pack,0,sizeof *pack);
}

bool cviii_whole_room_open(CviiiWholeRoomPack *pack, const char *path)
{
    if(!pack || !path || !*path) return false;
    FILE *f=fopen(path,"rb");
    if(!f) return false;
    uint8_t head[CVIII_WHOLE_ROOM_HEADER];
    if(fread(head,1,sizeof head,f)!=sizeof head ||
       memcmp(head,"CVROOM1\0",8)) { fclose(f); return false; }
    uint32_t size=u32(head+12);
    if(size<CVIII_WHOLE_ROOM_HEADER+
                CVIII_WHOLE_ROOM_COUNT*CVIII_WHOLE_ROOM_STRIDE ||
       size>131072u) { fclose(f); return false; }
    uint8_t *bytes=(uint8_t *)malloc(size);
    if(!bytes) { fclose(f); return false; }
    memcpy(bytes,head,sizeof head);
    size_t got=fread(bytes+sizeof head,1,size-sizeof head,f);
    int extra=fgetc(f),failed=ferror(f);
    fclose(f);
    CviiiWholeRoomPack *next=(CviiiWholeRoomPack *)calloc(1,sizeof *next);
    if(!next) { free(bytes); return false; }
    bool ok=got==size-sizeof head && extra==EOF && !failed &&
            cviii_whole_room_decode(bytes,size,next->rooms);
    if(!ok) { free(next); free(bytes); return false; }
    next->allocation=bytes;
    next->allocation_size=size;
    cviii_whole_room_close(pack);
    *pack=*next;
    free(next);
    return true;
}

bool cviii_whole_room_find(const CviiiWholeRoomPack *pack,
                           uint8_t stage,uint8_t area,uint8_t room,
                           CviiiWholeRoomView *out)
{
    if(!pack || !pack->allocation || !out) return false;
    unsigned key=((unsigned)stage<<16)|((unsigned)area<<8)|room;
    size_t lo=0,hi=CVIII_WHOLE_ROOM_COUNT;
    while(lo<hi) {
        size_t m=lo+(hi-lo)/2;
        const CviiiWholeRoomView *v=&pack->rooms[m];
        unsigned got=((unsigned)v->stage<<16)|((unsigned)v->area<<8)|v->room;
        if(got<key) lo=m+1;
        else hi=m;
    }
    if(lo==CVIII_WHOLE_ROOM_COUNT || pack->rooms[lo].stage!=stage ||
       pack->rooms[lo].area!=area || pack->rooms[lo].room!=room)
       return false;
    *out=pack->rooms[lo];
    return true;
}

bool cviii_whole_room_tile(const CviiiWholeRoomView *v,
                           unsigned x,unsigned y,uint8_t *tile)
{
    if(!v || !v->metatiles || !tile || x>=v->width || y>=v->height)
        return false;
    *tile=v->metatiles[(size_t)y*v->width+x];
    return true;
}