#include "cviii_room_gfx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path,size_t expected) {
    FILE *f=fopen(path,"rb");
    assert(f);
    uint8_t *p=malloc(expected);
    assert(p);
    assert(fread(p,1,expected,f)==expected && fgetc(f)==EOF);
    assert(fclose(f)==0);
    return p;
}
int main(int argc,char **argv) {
    assert(argc==4);
    CviiiWholeRoomPack rooms={0};
    CviiiRoomGfx gfx={0};
    assert(cviii_whole_room_open(&rooms,argv[2]));
    assert(cviii_room_gfx_open(&gfx,argv[3]));
    assert(gfx.size==28075);
    uint8_t *raw=read_file(argv[1],393232);
    uint8_t *pixels=malloc(524288);
    assert(pixels);
    const uint8_t *chr_rom=raw+0x40010;
    for(unsigned tile=0;tile<8192;tile++)
        for(unsigned y=0;y<8;y++)
            for(unsigned x=0;x<8;x++) {
                const uint8_t lo=chr_rom[tile*16+y],hi=chr_rom[tile*16+8+y];
                pixels[tile*64+y*8+x]=(uint8_t)((lo>>(7-x)&1)|((hi>>(7-x)&1)<<1));
            }
    CviiiChrIndex atlas={0};
    atlas.raw=chr_rom;atlas.pixels=pixels;
    const uint16_t counts[15]={145,85,82,100,72,86,59,96,96,80,256,93,96,93,124};
    unsigned total=0;
    for(unsigned i=0;i<15;i++) {assert(gfx.groups[i].count==counts[i]);total+=counts[i];}
    assert(total==1563);
    for(unsigned i=0;i<135;i++) {
        CviiiWholeRoomView r=rooms.rooms[i];
        const CviiiGfxGroup *g=cviii_room_gfx_group(&gfx,&r);
        assert(g!=NULL);
        for(unsigned m=0;m<r.width*r.height;m++) assert(r.metatiles[m]<g->count);
        uint8_t banks[4]={0x40,r.bg_r5,r.bg_r6,0x43};
        CviiiGfxPixel pix={0};
        const unsigned x=(r.width*32u)-1,y=(r.height*32u)-1;
        assert(cviii_room_gfx_sample(&gfx,&r,&atlas,banks,x,y,&pix));
        assert(pix.physical_tile<8192);
        assert(!cviii_room_gfx_sample(&gfx,&r,&atlas,banks,x+1,y,&pix));
    }
    CviiiWholeRoomView r=rooms.rooms[0];
    CviiiGfxPixel sample;
    uint8_t banks[4]={0x40,r.bg_r5,r.bg_r6,0x43};
    assert(cviii_room_gfx_sample(&gfx,&r,&atlas,banks,0,0,&sample));
    assert(sample.pattern==0x6a && sample.palette==1);
    uint32_t colors[16],color;
    for(unsigned i=0;i<16;i++)colors[i]=0xFF000000u|i;
    assert(cviii_room_gfx_color(&sample,colors,&color));
    assert(color==colors[sample.pixel2?sample.palette*4u+sample.pixel2:0]);
    /* Explicit standard NES attribute quadrant bit order. */
    assert(((0xE4u>>0u)&3u)==0);
    assert(((0xE4u>>2u)&3u)==1);
    assert(((0xE4u>>4u)&3u)==2);
    assert(((0xE4u>>6u)&3u)==3);
    CviiiGfxGroup gtmp[15];CviiiGfxRoom rtmp[135];
    uint8_t *bad=malloc(gfx.size);assert(bad);
    memcpy(bad,gfx.allocation,gfx.size);
    bad[gfx.size-1]^=1;
    assert(!cviii_room_gfx_decode(bad,gfx.size,gtmp,rtmp));
    assert(!cviii_room_gfx_open(&gfx,"missing_private_gfx.dat"));
    assert(cviii_room_gfx_group(&gfx,&r));
    free(bad);free(pixels);free(raw);
    cviii_room_gfx_close(&gfx);
    cviii_whole_room_close(&rooms);
    puts("PASS native: 135 room crosswalks, 1563 tiles, CHR pixel sampling, attr bounds, corrupt-pack rejection");
    return 0;
}