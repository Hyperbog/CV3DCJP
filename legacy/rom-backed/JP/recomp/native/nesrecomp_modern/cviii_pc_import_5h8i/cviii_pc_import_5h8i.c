#include "cviii_pc_import_5h8i.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned u16(const unsigned char *b) {return (unsigned)b[0]|(unsigned)b[1]<<8;}
static uint32_t u32(const unsigned char *b) {
    return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
}
static uint64_t u64(const unsigned char *b) {return u32(b)|((uint64_t)u32(b+4)<<32);}
static int64_t i64(const unsigned char *b) {
    const uint64_t v=u64(b);
    return v<=INT64_MAX?(int64_t)v:-(int64_t)(~v)-1;
}
static uint64_t digest(const unsigned char *p,size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++) {h^=p[i];h*=UINT64_C(1099511628211);}
    return h;
}
static const unsigned char expected_rom[32]={
  0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
  0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28
};

CviiiPcWorld *cviii_pc_world_open_5h8i(const char *filename) {
    if(!filename) return NULL;
    FILE *f=fopen(filename,"rb");
    if(!f) return NULL;
    unsigned char *data=NULL;
    CviiiPcWorld *world=NULL;
    if(fseek(f,0,SEEK_END)) goto done;
    long size=ftell(f);
    if(size<104 || size>16*1024*1024 || fseek(f,0,SEEK_SET)) goto done;
    data=(unsigned char*)malloc((size_t)size);
    if(!data || fread(data,1,(size_t)size,f)!=(size_t)size || ferror(f)) goto done;
    if(memcmp(data,"CVPCW01\0",8) || u16(data+8)!=1 ||
       u16(data+10)==0 || u16(data+10)>CVIII_PC_MAX_ROOMS ||
       u32(data+12)!=(uint32_t)size ||
       u64(data+16)!=digest(data+64,(size_t)size-64) ||
       u64(data+24)!=u64(data+16) ||
       memcmp(data+32,expected_rom,32)) goto done;
    unsigned count=u16(data+10);
    size_t next=64+(size_t)count*40;
    if(next>(size_t)size) goto done;
    world=cviii_pc_new(u64(data+24));
    if(!world) goto done;
    uint32_t prev=0;
    for(unsigned i=0;i<count;i++) {
        const unsigned char *p=data+64+(size_t)i*40;
        unsigned s=p[0],a=p[1],r=p[2],group=p[3];
        unsigned w=u16(p+4),h=u16(p+6),placed=p[8];
        int64_t x=i64(p+12),y=i64(p+20);
        uint32_t offset=u32(p+28),bytes=u32(p+32),source=u32(p+36);
        uint32_t key=(s<<16)|(a<<8)|r;
        if((i && key<=prev) || group>=15 ||
           !w || !h || (w%4) || (h%4) || (size_t)w*h>65536 ||
           placed>1 || p[9] || p[10] || p[11] ||
           !source || offset!=next || bytes!=(size_t)w*h*2 ||
           bytes>(size_t)size-next) goto invalid;
        prev=key;
        bool known=(s==0 && a==0 && r==0)||(s==0 && a==1 && r==0);
        if(placed!=(unsigned)known || (known &&
           (x!=(a==0?0:768) || y!=0)) || (!known && (x||y))) goto invalid;
        if(!cviii_pc_add_room(world,(uint8_t)s,(uint8_t)a,(uint8_t)r,
                              (uint16_t)w,(uint16_t)h)) goto invalid;
        CviiiPcRoom *room=cviii_pc_room(world,(uint8_t)s,(uint8_t)a,(uint8_t)r);
        if(placed && !cviii_pc_place_room(world,(uint8_t)s,(uint8_t)a,
                                         (uint8_t)r,x,y)) goto invalid;
        for(unsigned cy=0;cy<h;cy++)for(unsigned cx=0;cx<w;cx++) {
            unsigned tile=u16(data+next+((size_t)cy*w+cx)*2);
            /* Provenance: 1 + group*1024 + palette*256 + pattern. */
            if(tile<1+group*1024 || tile>1+group*1024+1023 ||
               !cviii_pc_set_base(room,CVIII_PC_TERRAIN,
                           (uint16_t)cx,(uint16_t)cy,(uint16_t)tile)) goto invalid;
        }
        next+=(size_t)bytes;
    }
    if(next!=(size_t)size) goto invalid;
    goto done;
invalid:
    cviii_pc_free(world);
    world=NULL;
done:
    free(data);
    fclose(f);
    return world;
}
