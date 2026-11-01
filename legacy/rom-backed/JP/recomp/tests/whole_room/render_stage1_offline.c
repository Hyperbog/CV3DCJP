/* OFFLINE Japanese ROM-derived Stage 1 geometry preview with synthetic palette.
   Not a reference emulator frame, not a screenshot of live gameplay. */
#include "cviii_room_renderer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(argc==5);
    CviiiWholeRoomPack rooms={0};CviiiRoomGfx gfx={0};
    assert(cviii_whole_room_open(&rooms,argv[2]));
    assert(cviii_room_gfx_open(&gfx,argv[3]));
    FILE *f=fopen(argv[1],"rb");assert(f);uint8_t *rom=malloc(393232);assert(rom);
    assert(fread(rom,1,393232,f)==393232 && fgetc(f)==EOF);fclose(f);
    uint8_t *pixels=malloc(8192u*64u);assert(pixels);
    for(unsigned t=0;t<8192;t++)for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
        unsigned lo=rom[0x40010+t*16+y],hi=rom[0x40010+t*16+8+y];
        pixels[t*64+y*8+x]=(uint8_t)(((lo>>(7-x))&1u)|(((hi>>(7-x))&1u)<<1));
    }
    CviiiChrIndex chr={0};chr.pixels=pixels;
    CviiiWholeRoomView room;assert(cviii_whole_room_find(&rooms,0,0,0,&room));
    CviiiRoomRenderSource source={0};source.graphics=&gfx;source.room=&room;source.chr=&chr;
    source.bg_bank_1k[0]=0x40;source.bg_bank_1k[1]=room.bg_r5;
    source.bg_bank_1k[2]=room.bg_r6;source.bg_bank_1k[3]=0x43;
    /* Deliberately grayscale synthetic palette: no invented live PPU palette. */
    source.palette_argb[0]=0xff101010;
    for(unsigned p=0;p<4;p++)for(unsigned v=1;v<4;v++){
        unsigned grey=55+v*53;
        source.palette_argb[p*4+v]=0xff000000|(grey<<16)|(grey<<8)|grey;
    }
    unsigned w=0,h=0;assert(cviii_room_render_dimensions(&source,&w,&h));
    assert(w==768 && h==192);
    uint32_t *argb=malloc((size_t)w*h*4);assert(argb);
    assert(cviii_room_render_view(&source,0,0,argb,w,h,w));
    FILE *out=fopen(argv[4],"wb");assert(out);
    assert(fwrite(argb,4,(size_t)w*h,out)==(size_t)w*h);
    assert(fclose(out)==0);
    printf("PASS offline Stage 1 room native ARGB: %ux%u bytes %zu\n",w,h,(size_t)w*h*4);
    free(argb);free(rom);free(pixels);
    cviii_room_gfx_close(&gfx);cviii_whole_room_close(&rooms);
    return 0;
}