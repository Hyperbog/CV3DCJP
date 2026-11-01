#include "cviii_room_preview.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *read_file(const char *path,size_t n) {
    FILE *f=fopen(path,"rb");assert(f);
    uint8_t *p=malloc(n);assert(p);
    assert(fread(p,1,n,f)==n && fgetc(f)==EOF);
    fclose(f);return p;
}
int main(int argc,char **argv) {
    assert(argc==4);
    CviiiWholeRoomPack rooms={0}; CviiiRoomGfx gfx={0};
    assert(cviii_whole_room_open(&rooms,argv[2]));
    assert(cviii_room_gfx_open(&gfx,argv[3]));
    uint8_t *rom=read_file(argv[1],393232);
    uint8_t *pixels=malloc(8192u*64u);assert(pixels);
    for(unsigned tile=0;tile<8192;tile++)for(unsigned y=0;y<8;y++)
        for(unsigned x=0;x<8;x++) {
            unsigned lo=rom[0x40010+tile*16+y],hi=rom[0x40010+tile*16+8+y];
            pixels[tile*64+y*8+x]=(uint8_t)((lo>>(7-x)&1u)|((hi>>(7-x)&1u)<<1));
        }
    CviiiChrIndex atlas={0};atlas.pixels=pixels;
    uint32_t *left=malloc(256u*240u*4u),*both=malloc(512u*240u*4u);
    assert(left && both);
    for(unsigned n=0;n<256u*240u;n++)left[n]=0xff800000u+(n&0xffffu);
    CviiiRoomRenderSource src={0};src.graphics=&gfx;src.chr=&atlas;
    src.room=&rooms.rooms[0];
    src.bg_bank_1k[0]=0x40;src.bg_bank_1k[1]=src.room->bg_r5;
    src.bg_bank_1k[2]=src.room->bg_r6;src.bg_bank_1k[3]=0x43;
    for(unsigned i=0;i<16;i++)src.palette_argb[i]=0xff000000u+i;
    assert(cviii_room_preview_compose(&src,left,0,0,both));
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)
        assert(both[y*512u+x]==left[y*256u+x]);
    assert(memcmp(left,both,4)==0);
    unsigned hash=0;
    for(unsigned y=0;y<240;y++)for(unsigned x=256;x<512;x++)
        hash=hash*33u+both[y*512u+x];
    assert(hash);
    assert(!cviii_room_preview_compose(NULL,left,0,0,both));
    assert(!cviii_room_preview_compose(&src,NULL,0,0,both));
    assert(!cviii_room_preview_compose(&src,left,0,0,NULL));
    free(left);free(both);free(rom);free(pixels);
    cviii_room_gfx_close(&gfx);cviii_whole_room_close(&rooms);
    printf("PASS preview: exact native left half, native room right half (hash %08x)\n",hash);
    return 0;
}