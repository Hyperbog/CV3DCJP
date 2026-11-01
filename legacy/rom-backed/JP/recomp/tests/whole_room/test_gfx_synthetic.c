/* ROM-free reproducible regression: structure, all four attribute quadrants,
 * dynamic bank input, universal backdrop and invalid pack handling. */
#include "cviii_room_gfx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HEAD 64u
#define GROUP_RECORD 24u
#define ROOM_RECORD 8u
#define BASE (HEAD+15u*GROUP_RECORD+135u*ROOM_RECORD)

static void wr16(uint8_t *p,unsigned x) {p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void wr32(uint8_t *p,uint32_t x) {
    for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(8*i));
}
static void wr64(uint8_t *p,uint64_t x) {
    wr32(p,(uint32_t)x);wr32(p+4,(uint32_t)(x>>32));
}
static uint64_t hash(const uint8_t *p,size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}
    return h;
}
static uint8_t selector_group(unsigned s,unsigned r) {
    if(s==13 && r<2)return 2;
    if(s==14 && r==1)return 1;
    return (uint8_t)s;
}
int main(void) {
    const uint16_t counts[15]={145,85,82,100,72,86,59,96,96,80,256,93,96,93,124};
    const uint8_t bank[15]={8,8,8,8,8,7,7,7,7,6,6,7,6,6,6};
    const uint8_t per_stage[15]={8,18,10,12,9,6,6,10,7,5,14,7,5,11,7};
    const uint8_t source_sha[32]={
        0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
        0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28};
    const unsigned length=BASE+1563u*17u;
    uint8_t *buffer=(uint8_t *)calloc(length,1);
    assert(buffer);
    memcpy(buffer,"CVGFX01\0",8);
    wr16(buffer+8,1);wr16(buffer+10,15);wr16(buffer+12,135);
    wr32(buffer+16,length);wr32(buffer+20,BASE);
    memcpy(buffer+24,source_sha,32);
    unsigned cursor=BASE,index=0;
    for(unsigned s=0;s<15;s++) {
        uint8_t *p=buffer+HEAD+s*GROUP_RECORD;
        p[0]=(uint8_t)s;p[1]=bank[s];wr16(p+2,counts[s]);
        unsigned original=0x20011u+s*0x80u;
        wr32(p+4,original);wr32(p+8,original+counts[s]*16u);
        wr32(p+12,cursor);cursor+=counts[s]*16u;
        wr32(p+16,cursor);
        if(s==0)buffer[cursor]=0xE4u;
        cursor+=counts[s];
        for(unsigned r=0;r<per_stage[s];r++) {
            uint8_t *record=buffer+HEAD+15u*GROUP_RECORD+index*ROOM_RECORD;
            record[0]=(uint8_t)s;record[1]=0;record[2]=(uint8_t)r;
            record[3]=selector_group(s,r);
            wr32(record+4,0x200bau+index);
            index++;
        }
    }
    assert(index==135 && cursor==length);
    wr64(buffer+56,hash(buffer+HEAD,length-HEAD));
    CviiiGfxGroup g[15];CviiiGfxRoom refs[135];
    assert(cviii_room_gfx_decode(buffer,length,g,refs));
    CviiiRoomGfx pack={0};
    pack.allocation=buffer;pack.size=length;
    memcpy(pack.groups,g,sizeof g);memcpy(pack.rooms,refs,sizeof refs);
    const uint8_t mt=0;
    CviiiWholeRoomView room={0};
    room.stage=0;room.area=0;room.room=0;
    room.source_rom_offset=0x200ba;room.width=1;room.height=1;room.metatiles=&mt;
    assert(cviii_room_gfx_group(&pack,&room)==&pack.groups[0]);
    uint8_t *pixels=(uint8_t *)calloc(524288u,1);
    assert(pixels);
    pixels[0x40u*64u*64u]=1;
    pixels[0x41u*64u*64u]=2;
    CviiiChrIndex chr={0};chr.pixels=pixels;
    uint8_t active[4]={0x40,0x45,0x47,0x43};
    CviiiGfxPixel pix;
    for(unsigned q=0;q<4;q++) {
        unsigned x=(q%2u)*16u,y=(q/2u)*16u;
        assert(cviii_room_gfx_sample(&pack,&room,&chr,active,x,y,&pix));
        assert(pix.palette==q && pix.pattern==0);
    }
    assert(cviii_room_gfx_sample(&pack,&room,&chr,active,0,0,&pix));
    assert(pix.pixel2==1 && pix.physical_tile==0x40u*64u);
    uint32_t colors[16],color;
    for(unsigned k=0;k<16;k++)colors[k]=0xff000000u|k;
    assert(cviii_room_gfx_color(&pix,colors,&color) && color==colors[1]);
    active[0]=0x41;
    assert(cviii_room_gfx_sample(&pack,&room,&chr,active,0,0,&pix));
    assert(pix.pixel2==2 && pix.physical_tile==0x41u*64u);
    assert(cviii_room_gfx_color(&pix,colors,&color) && color==colors[2]);
    active[0]=0x80;
    assert(!cviii_room_gfx_sample(&pack,&room,&chr,active,0,0,&pix));
    assert(!cviii_room_gfx_sample(&pack,&room,&chr,active,32,0,&pix));
    buffer[length-1]^=1;
    assert(!cviii_room_gfx_decode(buffer,length,g,refs));
    free(pixels);cviii_room_gfx_close(&pack);
    puts("PASS ROM-free: 15 groups, 135 bindings, four palette quadrants, dynamic bank, rejects damage");
    return 0;
}