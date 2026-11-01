/* Tests ROM-backed full-room pixels against independently sampled pixels. */
#include "cviii_room_renderer.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *file_read(const char *path, size_t expected)
{
    FILE *f=fopen(path,"rb"); assert(f);
    uint8_t *p=malloc(expected); assert(p);
    assert(fread(p,1,expected,f)==expected && fgetc(f)==EOF);
    assert(fclose(f)==0); return p;
}
static uint8_t *chr_decode(const uint8_t *chr)
{
    uint8_t *out=malloc(8192u*64u); assert(out);
    for (unsigned tile=0; tile<8192; ++tile)
        for (unsigned y=0; y<8; ++y)
            for (unsigned x=0; x<8; ++x) {
                unsigned lo=chr[tile*16+y], hi=chr[tile*16+8+y];
                out[tile*64+y*8+x]=(uint8_t)(((lo>>(7-x))&1u)|(((hi>>(7-x))&1u)<<1));
            }
    return out;
}
int main(int argc, char **argv)
{
    assert(argc==4);
    uint8_t *raw=file_read(argv[1],393232);
    uint8_t *decoded=chr_decode(raw+0x40010);
    CviiiChrIndex atlas={0}; atlas.raw=raw+0x40010; atlas.pixels=decoded;
    CviiiWholeRoomPack rooms={0}; CviiiRoomGfx gfx={0};
    assert(cviii_whole_room_open(&rooms,argv[2]));
    assert(cviii_room_gfx_open(&gfx,argv[3]));
    uint32_t colors[16];
    for(unsigned i=0;i<16;i++)colors[i]=0xff020000u+i*0x00070103u;
    uint32_t output[320*240];
    unsigned checked=0;
    for(unsigned i=0;i<135;i++) {
        CviiiWholeRoomView *r=&rooms.rooms[i];
        CviiiRoomRenderSource src={0};
        src.graphics=&gfx;src.room=r;src.chr=&atlas;
        src.bg_bank_1k[0]=0x40; src.bg_bank_1k[1]=r->bg_r5;
        src.bg_bank_1k[2]=r->bg_r6;src.bg_bank_1k[3]=0x43;
        memcpy(src.palette_argb,colors,sizeof colors);
        unsigned fullw=0,fullh=0;
        assert(cviii_room_render_dimensions(&src,&fullw,&fullh));
        assert(fullw==r->width*32u && fullh==r->height*32u);
        const unsigned camera_x=(i&1u) ? fullw/2u : 0u;
        const unsigned camera_y=(i&2u) ? fullh/2u : 0u;
        assert(cviii_room_render_view(&src,camera_x,camera_y,output,320,240,320));
        /* Compare each corner and several varying positions with the
         * independent older pixel sampler + attribute color resolver. */
        const unsigned positions[8][2]={{0,0},{31,31},{127,111},{255,239},
                                         {319,0},{0,239},{191,128},{319,239}};
        for(unsigned n=0;n<8;n++) {
            unsigned x=positions[n][0],y=positions[n][1];
            unsigned wx=x+camera_x,wy=y+camera_y;
            uint32_t want=colors[0];
            if(wx<fullw && wy<fullh) {
                CviiiGfxPixel p={0};
                assert(cviii_room_gfx_sample(&gfx,r,&atlas,src.bg_bank_1k,wx,wy,&p));
                assert(cviii_room_gfx_color(&p,colors,&want));
            }
            assert(output[(size_t)y*320+x]==want);
            ++checked;
        }
    }
    CviiiRoomRenderSource first={0};
    first.graphics=&gfx;first.room=&rooms.rooms[0];first.chr=&atlas;
    first.bg_bank_1k[0]=0x40;first.bg_bank_1k[1]=0x45;
    first.bg_bank_1k[2]=0x47;first.bg_bank_1k[3]=0x43;
    memcpy(first.palette_argb,colors,sizeof colors);
    assert(cviii_room_render_view(&first,0,0,output,320,240,320));
    /* Render an entire 3-screen room beyond the original 256-pixel width. */
    unsigned full_width=0,full_height=0;
    assert(cviii_room_render_dimensions(&first,&full_width,&full_height));
    assert(full_width==768 && full_height==192);
    uint32_t *whole=malloc((size_t)full_width*full_height*sizeof(uint32_t));
    assert(whole);
    assert(cviii_room_render_view(&first,0,0,whole,full_width,full_height,full_width));
    for(unsigned x=0;x<full_width;x+=53u) {
        CviiiGfxPixel p={0}; uint32_t want;
        assert(cviii_room_gfx_sample(&gfx,first.room,&atlas,first.bg_bank_1k,x,191,&p));
        assert(cviii_room_gfx_color(&p,colors,&want));
        assert(whole[191u*full_width+x]==want);
    }
    free(whole);
    uint32_t before=output[0];
    first.bg_bank_1k[0]=128;
    assert(!cviii_room_render_view(&first,0,0,output,320,240,320));
    assert(output[0]==before);
    first.bg_bank_1k[0]=0x40;
    assert(!cviii_room_render_view(&first,0,0,output,320,240,319));
    assert(output[0]==before);
    assert(cviii_room_render_view(&first,UINT32_MAX,UINT32_MAX,output,320,240,320));
    for(unsigned i=0;i<320*240;i++)assert(output[i]==colors[0]);
    free(raw);free(decoded);
    cviii_room_gfx_close(&gfx);cviii_whole_room_close(&rooms);
    printf("PASS native whole-room: 135 rooms, %u pixel comparisons, viewport and rejection\n",checked);
    return 0;
}