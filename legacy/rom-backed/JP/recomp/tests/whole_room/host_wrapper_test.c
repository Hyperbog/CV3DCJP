
/* Compiles the exact installed comparison wrapper, with a synthetic host.
 * This is NOT the full original NESRecomp runtime or gameplay. */
#include "cviii_room_preview.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int enhanced;
    CviiiWholeRoomPack rooms;
    CviiiRoomGfx gfx;
    CviiiChrIndex assets;
    CviiiWholeRoomView active_room;
    const CviiiGfxGroup *active_tileset;
    int active_room_valid;
    int room_preview;
    uint32_t room_preview_pixels[CVIII_ROOM_PREVIEW_W*CVIII_ROOM_PREVIEW_H];
} CviiiEdition;
static uint8_t synthetic_ram[2048];
static uint32_t original[256*240];
static uint32_t palette[16];
static const uint8_t *cyc_cpu_ram(void) { return synthetic_ram; }
static uint32_t cyc_render_color(int entry) { return palette[entry&15]; }
static const uint32_t *present(void *unused,int *w,int *h) {
    (void)unused;
    *w=256; *h=240;
    return original;
}
static const uint32_t *present_room_comparison(void *context,int *width,int *height)
{
    CviiiEdition *settings=(CviiiEdition *)context;
    const uint32_t *original=present(context,width,height);
    if(!settings->enhanced || !settings->room_preview || !original ||
       *width!=256 || *height!=240 || !settings->active_room_valid ||
       !settings->active_tileset || !settings->assets.pixels)
        return original;
    CviiiRoomRenderSource source={0};
    source.graphics=&settings->gfx;
    source.room=&settings->active_room;
    source.chr=&settings->assets;
    source.bg_bank_1k[0]=0x40; /* room initialization defaults only */
    source.bg_bank_1k[1]=settings->active_room.bg_r5;
    source.bg_bank_1k[2]=settings->active_room.bg_r6;
    source.bg_bank_1k[3]=0x43;
    for(unsigned i=0;i<16u;i++) source.palette_argb[i]=cyc_render_color((int)i);
    unsigned camera=0;
    const uint8_t *ram=cyc_cpu_ram();
    if(ram) camera=((unsigned)ram[0x57]<<8)|ram[0x56];
    unsigned camera_x=(settings->active_room.metadata&0x80u)?0u:camera;
    unsigned camera_y=(settings->active_room.metadata&0x80u)?camera:0u;
    if(!cviii_room_preview_compose(&source,original,camera_x,camera_y,
                                   settings->room_preview_pixels))
        return original;
    *width=(int)CVIII_ROOM_PREVIEW_W;
    *height=(int)CVIII_ROOM_PREVIEW_H;
    return settings->room_preview_pixels;
}


static void select_room(CviiiEdition *s) {
    s->active_room_valid=cviii_whole_room_find(&s->rooms,
            synthetic_ram[0x34],synthetic_ram[0x35],synthetic_ram[0x36],
            &s->active_room);
    s->active_tileset=s->active_room_valid ?
        cviii_room_gfx_group(&s->gfx,&s->active_room) : NULL;
}
int main(int argc,char **argv) {
    assert(argc==4);
    CviiiEdition *s=calloc(1,sizeof *s); assert(s);
    FILE *f=fopen(argv[1],"rb"); assert(f);
    uint8_t *rom=malloc(393232);assert(rom);
    assert(fread(rom,1,393232,f)==393232 && fgetc(f)==EOF);fclose(f);
    uint8_t *pixels=malloc(8192u*64u);assert(pixels);
    for(unsigned t=0;t<8192;t++)for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++) {
        uint8_t lo=rom[0x40010u+t*16u+y];
        uint8_t hi=rom[0x40010u+t*16u+8u+y];
        pixels[t*64u+y*8u+x]=(uint8_t)(((lo>>(7-x))&1u)|(((hi>>(7-x))&1u)<<1));
    }
    s->assets.pixels=pixels;
    for(unsigned i=0;i<256*240;i++)original[i]=0xff102030u+((i*13u)&0xffffffu);
    uint32_t oldhash=0;
    for(unsigned i=0;i<256*240;i++)oldhash=(oldhash*33u)^original[i];
    for(unsigned i=0;i<16;i++)palette[i]=0xff000000u|((i*29u)<<16)|((i*13u)<<8)|(i*11u);
    int w=0,h=0;
    const uint32_t *p=present_room_comparison(s,&w,&h);
    assert(p==original && w==256 && h==240); /* disabled */
    s->enhanced=1;s->room_preview=1;
    p=present_room_comparison(s,&w,&h);
    assert(p==original && w==256 && h==240); /* missing resources */
    assert(cviii_whole_room_open(&s->rooms,argv[2]));
    assert(cviii_room_gfx_open(&s->gfx,argv[3]));
    p=present_room_comparison(s,&w,&h);
    assert(p==original && w==256 && h==240); /* unbound */
    select_room(s);
    assert(s->active_room_valid && s->active_tileset && s->active_room.width==24);
    uint8_t before[2048];memcpy(before,synthetic_ram,sizeof before);
    p=present_room_comparison(s,&w,&h);
    assert(p==s->room_preview_pixels && w==512 && h==240);
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<256;x++)
        assert(p[y*512+x]==original[y*256+x]);
    uint32_t right_hash=0;
    for(unsigned y=0;y<240;y++)for(unsigned x=256;x<512;x++)
        right_hash=(right_hash*33u)^p[y*512+x];
    assert(right_hash!=0);
    assert(memcmp(before,synthetic_ram,sizeof before)==0);
    uint32_t nowhash=0;
    for(unsigned i=0;i<256*240;i++)nowhash=(nowhash*33u)^original[i];
    assert(oldhash==nowhash);
    synthetic_ram[0x56]=38;
    p=present_room_comparison(s,&w,&h);
    uint32_t moved_hash=0;
    for(unsigned y=0;y<240;y++)for(unsigned x=256;x<512;x++)
        moved_hash=(moved_hash*33u)^p[y*512+x];
    assert(right_hash!=moved_hash);
    synthetic_ram[0x56]=0;
    synthetic_ram[0x34]=255;
    select_room(s);
    p=present_room_comparison(s,&w,&h);
    assert(p==original && w==256 && h==240); /* invalid room */
    synthetic_ram[0x34]=0;
    select_room(s);
    s->enhanced=0;
    p=present_room_comparison(s,&w,&h);
    assert(p==original && w==256 && h==240); /* original mode */
    s->enhanced=1; s->room_preview=0;
    p=present_room_comparison(s,&w,&h);
    assert(p==original && w==256 && h==240); /* disabled */
    cviii_room_gfx_close(&s->gfx);cviii_whole_room_close(&s->rooms);
    free(pixels);free(rom);free(s);
    printf("PASS exact installed preview wrapper in synthetic host: 512x240, fallback, scroll, ROM binding; hash %08x\\n",right_hash);
    return 0;
}