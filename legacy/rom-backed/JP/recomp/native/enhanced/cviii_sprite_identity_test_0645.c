/* JP0645: original PRG26 OAM-rotation gameplay sprites, ROM-free matrix.
 * Exercises actual opt-in C11 sprite presentation: never affects NES state. */
#include "cviii_sprite_layers_0608.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t chr(void *user,uint16_t addr)
{
    (void)user;
    const unsigned tile=(unsigned)addr/16u;
    const unsigned plane=(unsigned)addr%16u;
    if(tile==1u && plane<8u)return 0xFFu; /* color 1 */
    if(tile==2u && plane>=8u)return 0xFFu; /* color 2 */
    if(tile==3u)return 0xFFu; /* color 3, animation */
    return 0u;
}
static uint32_t palette(void *user,unsigned index)
{
    (void)user;
    return index;
}
static void clear_oam(uint8_t oam[256]) {memset(oam,0xFFu,256u);}
static void set_sprite(uint8_t oam[256],unsigned slot,
                       uint8_t y,uint8_t tile,uint8_t attr,uint8_t x)
{
    assert(slot<64u);
    oam[slot*4u]=y;
    oam[slot*4u+1u]=tile;
    oam[slot*4u+2u]=attr;
    oam[slot*4u+3u]=x;
}
static void capture(CviiiSpriteLayers *layers,const uint8_t oam[256],
                    uint8_t phase,const uint8_t mask[240])
{
    const uint8_t tall[240]={0};
    const uint16_t tables[240]={0};
    const CviiiLayerFns fn={chr,palette,NULL};
    cviii_layers_capture(layers,oam,phase,mask,tall,tables,&fn);
}
static uint32_t pixel_at(CviiiSpriteLayers *layers,
                         uint32_t frame[256u*240u],
                         const uint8_t mask[240],unsigned x,unsigned y)
{
    const CviiiLayerFns fn={chr,palette,NULL};
    static const uint8_t bg[256u*240u]={0};
    memset(frame,0,256u*240u*sizeof(*frame));
    cviii_layers_draw(layers,frame,bg,mask,&fn);
    return frame[y*256u+x];
}
int main(void)
{
    CviiiSpriteLayers *s=calloc(1,sizeof(*s));
    uint32_t *frame=calloc(256u*240u,sizeof(*frame));
    uint8_t mask[240],oam[256];
    assert(s && frame);
    memset(mask,0x14,sizeof(mask));

    /* Emission index 0: slot0 (phase0) becomes slot17 (phase0x44).
     * Emission index 1: slot49 becomes slot2. Tiles 1->3 and X/Y
     * move by 2 while phase and sprite flip change. No old ghost. */
    clear_oam(oam);
    set_sprite(oam,0,39,1,0,40);
    set_sprite(oam,49,39,2,1,40);
    capture(s,oam,0,mask);
    assert(s->held_now==0 && s->frames==1u);
    assert(pixel_at(s,frame,mask,40,40)==17u);
    clear_oam(oam);
    set_sprite(oam,17,41,3,0x40,42);
    set_sprite(oam,2,39,2,1,40);
    capture(s,oam,0x44,mask);
    assert(s->held_now==0 && s->phase==0x44);
    assert(pixel_at(s,frame,mask,42,42)==19u);

    /* Disappeared second sprite must NOT be consumed by the same
     * surviving lookalike: one-to-one matching, one held sprite. */
    cviii_layers_reset(s);
    clear_oam(oam);
    set_sprite(oam,0,39,1,0,40);
    set_sprite(oam,49,39,1,0,40);
    capture(s,oam,0,mask);
    clear_oam(oam);
    set_sprite(oam,17,39,1,0,40);
    capture(s,oam,0x44,mask);
    assert(s->held_now==1u && s->held[49].valid);
    assert(!s->held[0].valid);
    assert(pixel_at(s,frame,mask,40,40)==17u);
    capture(s,oam,0x44,mask);
    assert(s->held_now==0u); /* never retains for two frames */

    /* All 64 logical emission positions rotate between real physical
     * slots and all tiles animate simultaneously. No one-frame ghost
     * should survive from a full OAM frame in this valid permutation. */
    cviii_layers_reset(s);
    clear_oam(oam);
    for(unsigned emission=0;emission<64u;++emission) {
        const unsigned physical=(emission*49u)&63u;
        set_sprite(oam,physical,39,1,0,(uint8_t)(emission*4u));
    }
    capture(s,oam,0,mask);
    clear_oam(oam);
    for(unsigned emission=0;emission<64u;++emission) {
        const unsigned physical=(17u+emission*49u)&63u;
        set_sprite(oam,physical,39,3,0,(uint8_t)(emission*4u));
    }
    capture(s,oam,0x44u,mask);
    assert(s->held_now==0u);
    assert(pixel_at(s,frame,mask,16,40)==19u);

    /* A non-OAM-word-aligned phase must never yield a fabricated
     * logical identity for a changing tile. Conservative fallback. */
    cviii_layers_reset(s);
    clear_oam(oam);
    set_sprite(oam,0,39,1,0,40);
    capture(s,oam,3u,mask);
    clear_oam(oam);
    set_sprite(oam,1,39,3,0,40);
    capture(s,oam,4u,mask);
    assert(s->held_now==1u && s->held[0].valid);

    /* Offsets wrapping past 0xFF are still a permutation of 64 slots.
     * Phase FC -> 40 follows original +44 shift and actor stays put. */
    cviii_layers_reset(s);
    clear_oam(oam);
    set_sprite(oam,63,39,1,0,40);
    capture(s,oam,0xFCu,mask);
    clear_oam(oam);
    set_sprite(oam,16,39,3,0,40);
    capture(s,oam,0x40u,mask);
    assert(s->held_now==0u);

    /* Mass removal suggests scene change: no lingering actors. */
    cviii_layers_reset(s);
    clear_oam(oam);
    for(unsigned i=0;i<16u;i++)
        set_sprite(oam,i,60,1,0,(uint8_t)(i*12u));
    capture(s,oam,0,mask);
    clear_oam(oam);
    set_sprite(oam,17,60,1,0,0);
    capture(s,oam,0x44,mask);
    assert(s->held_now==0u);

    /* State load, power-on and scene change clear optional history. */
    cviii_layers_reset(s);
    assert(!s->primed && s->held_now==0u &&
           s->frames==0u && s->total_held==0u);

    /* Samples never substitute gameplay sprites when PPU masking
     * disables sprites; original OAM/CPU snapshots are unchanged. */
    clear_oam(oam);
    set_sprite(oam,0,39,1,0,40);
    mask[40]=0x04;
    capture(s,oam,0,mask);
    assert(s->held_now==0u);
    assert(pixel_at(s,frame,mask,40,40)==0u);

    free(frame);
    free(s);
    puts("PASS JP0645 rotated OAM identity: animation, overlap, fallback, wrap, scene, reset and mask");
    return 0;
}
