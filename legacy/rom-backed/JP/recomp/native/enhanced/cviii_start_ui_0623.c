/* JP0623: original game CHR glyphs, isolated host-level modal presentation. */
#include "cviii_start_ui_0623.h"
#include <stddef.h>
#define SCREEN_W 256u
#define SCREEN_H 240u
#define PANEL_X 8u
#define PANEL_Y 40u
#define PANEL_W 240u
#define PANEL_H 168u
#define FONT_ZERO 0x1041u
#define FONT_A 0x1050u
static unsigned tile_for(char c)
{
    if(c>='A'&&c<='Z')return FONT_A+(unsigned)(c-'A');
    if(c>='0'&&c<='9')return FONT_ZERO+(unsigned)(c-'0');
    return 0;
}
static void print_at(uint32_t *pixels,const unsigned char *chr,
                     unsigned x,unsigned y,const char *s,int selected)
{
    for(;s&&*s&&x+8<=PANEL_X+PANEL_W;s++,x+=8) {
        unsigned tile=tile_for(*s);
        if(!tile||y+8>SCREEN_H)continue;
        const unsigned char *src=chr+tile*64u;
        for(unsigned row=0;row<8u;row++)
            for(unsigned col=0;col<8u;col++) {
                unsigned pixel=src[row*8u+col];
                if(pixel)
                    pixels[(y+row)*SCREEN_W+x+col]=
                        selected ? UINT32_C(0xFFFF882C) :
                        UINT32_C(0xFFF1EEEA);
            }
    }
}
int cviii_start_ui_draw(uint32_t *pixels,unsigned w,unsigned h,
                        const CviiiChrIndex *assets,const CviiiStartMenu *menu)
{
    if(!pixels||w!=SCREEN_W||h!=SCREEN_H||!assets||
       !assets->pixels||!menu||!menu->open||!menu->count||
       menu->count>CVIII_START_MAX||menu->selected>=menu->count)return 0;
    const unsigned char *chr=assets->pixels;
    for(unsigned y=PANEL_Y;y<PANEL_Y+PANEL_H;y++)
        for(unsigned x=PANEL_X;x<PANEL_X+PANEL_W;x++)
            pixels[y*SCREEN_W+x]=UINT32_C(0xFF09070F);
    print_at(pixels,chr,76,52,"PAUSE MENU",0);
    /* Scroll entries instead of ever assuming a fixed four-row menu. */
    const size_t first=menu->selected>4u?menu->selected-4u:0u;
    for(size_t row=0;row<6u && first+row<menu->count;row++) {
        const size_t idx=first+row;
        const CviiiStartEntry *it=&menu->entries[idx];
        const unsigned y=79u+(unsigned)row*17u;
        print_at(pixels,chr,32,y,it->label,(int)(idx==menu->selected));
    }
    if(menu->quit_armed)
        print_at(pixels,chr,40,183,"QUIT ENTER AGAIN",1);
    else
        print_at(pixels,chr,40,183,"ENTER SELECT",0);
    print_at(pixels,chr,44,196,"F4 ESC CLOSE",0);
    return 1;
}

/* JP0625: original-font slot selection, shared panel and authenticated CHR. */
int cviii_start_slot_ui_draw(uint32_t *pixels,unsigned w,unsigned h,
                             const CviiiChrIndex *assets,
                             const CviiiSlotPicker *picker)
{
    if(!pixels || w!=SCREEN_W || h!=SCREEN_H || !assets ||
       !assets->pixels || !cviii_slot_active(picker)) return 0;
    const unsigned char *chr=assets->pixels;
    for(unsigned y=PANEL_Y;y<PANEL_Y+PANEL_H;y++)
        for(unsigned x=PANEL_X;x<PANEL_X+PANEL_W;x++)
            pixels[y*SCREEN_W+x]=UINT32_C(0xFF09070F);
    print_at(pixels,chr,80u,60u,
             picker->mode==CVIII_START_SAVE?"SAVE SLOT":"LOAD SLOT",0);
    const char digit[2]={(char)('0'+picker->slot),'\0'};
    print_at(pixels,chr,124u,96u,digit,1);
    print_at(pixels,chr,48u,139u,"LEFT RIGHT SLOT",0);
    print_at(pixels,chr,48u,162u,"ENTER CONFIRM",0);
    print_at(pixels,chr,84u,186u,"ESC BACK",0);
    return 1;
}
