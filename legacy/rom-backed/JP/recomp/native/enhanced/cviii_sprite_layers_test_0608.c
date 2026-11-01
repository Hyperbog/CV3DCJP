/* ROM-free independent JP0608 temporal sprite rendering regression. */
#include "cviii_sprite_layers_0608.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t read_chr(void *ctx,uint16_t a) {
    (void)ctx;
    if(a/16==1u && a%16<8u)return 255;
    if(a/16==2u && a%16>=8u)return 255;
    if(a/16==3u)return 255;
    return 0;
}
static uint32_t color(void *ctx,unsigned value){(void)ctx;return value;}
static void add(uint8_t *o,unsigned slot,uint8_t y,uint8_t tile,uint8_t attr,uint8_t x){
    o[slot*4]=y;o[slot*4+1]=tile;o[slot*4+2]=attr;o[slot*4+3]=x;
}
static void blank(uint8_t *o){memset(o,0xFF,256);}
int main(void){
    CviiiSpriteLayers *s=calloc(1,sizeof(*s));
    uint32_t *canvas=calloc(256*240,sizeof(uint32_t));
    uint8_t *bg=calloc(256*240,1);
    uint8_t mask[240],tall[240]={0},oam[256];
    uint16_t tables[240]={0};
    if(!s||!canvas||!bg)return 1;
    memset(mask,0x14,sizeof(mask));
    CviiiLayerFns f={read_chr,color,NULL};
    blank(oam);add(oam,0,39,1,0,40);add(oam,1,39,2,1,40);
    cviii_layers_capture(s,oam,0,mask,tall,tables,&f);
    if(s->held_now || s->frames!=1)return 2;
    cviii_layers_draw(s,canvas,bg,mask,&f);
    if(canvas[40*256+40]!=17u)return 3;
    blank(oam);add(oam,1,39,2,1,40);
    cviii_layers_capture(s,oam,0,mask,tall,tables,&f);
    if(s->held_now!=1 || !s->held[0].valid)return 4;
    memset(canvas,0,256*240*sizeof(uint32_t));
    cviii_layers_draw(s,canvas,bg,mask,&f);
    /* Current live sprites must always appear over old rescued pieces. */
    if(canvas[40*256+40]!=22u)return 5;
    cviii_layers_capture(s,oam,0,mask,tall,tables,&f);
    if(s->held_now || s->held[0].valid)return 6;
    cviii_layers_reset(s);
    blank(oam);add(oam,0,39,1,0x20,40);
    cviii_layers_capture(s,oam,0,mask,tall,tables,&f);
    memset(canvas,0,256*240*sizeof(uint32_t));bg[40*256+40]=1;
    cviii_layers_draw(s,canvas,bg,mask,&f);
    if(canvas[40*256+40]!=0)return 7;
    bg[40*256+40]=0;
    blank(oam);add(oam,0,39,1,0,0);
    cviii_layers_capture(s,oam,0,mask,tall,tables,&f);
    mask[40]=0x10;memset(canvas,0,256*240*sizeof(uint32_t));
    cviii_layers_draw(s,canvas,bg,mask,&f);
    if(canvas[40*256]!=0)return 8;
    mask[40]=0x14;
    blank(oam);
    for(unsigned i=0;i<16;++i)add(oam,i,60,1,0,(uint8_t)(i*12));
    cviii_layers_capture(s,oam,0,mask,tall,tables,&f);
    blank(oam);cviii_layers_capture(s,oam,0,mask,tall,tables,&f);
    if(s->held_now || s->total_held<1)return 9;

    /* The original game changes OAM phase by +$44 each frame. At phase
     * 0, emission #0 lands in physical slot 0, emission #1 in slot 49.
     * At phase $44, the SAME logical actors land in slot 17 and slot 2.
     * Hardware OAM priority reverses! The Enhanced paint order must not.
     */
    cviii_layers_reset(s); blank(oam);
    add(oam,0,39,1,0,40); /* Trevor: color index 1 */
    add(oam,49,39,2,1,40); /* Torch: color index 2 */
    cviii_layers_capture(s,oam,0,mask,tall,tables,&f);
    memset(canvas,0,256*240*sizeof(uint32_t));
    cviii_layers_draw(s,canvas,bg,mask,&f);
    if(canvas[40*256+40]!=17u)return 10;
    blank(oam);
    add(oam,17,39,1,0,40);
    add(oam,2,39,2,1,40);
    cviii_layers_capture(s,oam,0x44u,mask,tall,tables,&f);
    memset(canvas,0,256*240*sizeof(uint32_t));
    cviii_layers_draw(s,canvas,bg,mask,&f);
    if(canvas[40*256+40]!=17u || s->phase!=0x44u || s->prior_phase!=0)return 11;
    cviii_layers_reset(s);
    if(s->primed || s->total_held || s->frames)return 12;
    free(canvas);free(bg);free(s);
    puts("PASS JP0608 separate OAM layers, one-frame sprite hold, priorities, clipping and reset");
    return 0;
}
