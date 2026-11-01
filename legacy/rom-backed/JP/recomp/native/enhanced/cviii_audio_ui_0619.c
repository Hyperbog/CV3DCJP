/* JP0619: source graphic tiles remain in owner-private authenticated CHR index.
 * Gameplay pixels are a presentation input; no emulated PPU writes occur. */
#include "cviii_audio_ui_0619.h"

#define PANEL_X 8u
#define PANEL_Y 44u
#define PANEL_W 240u
#define PANEL_H 176u
#define BAR_X 80u
#define BAR_W 144u
#define BAR_TILES (BAR_W/8u)
#define TEXT_X 16u
#define VALUE_X 224u
#define ROW_Y 88u
#define ROW_STEP 32u
/* Palette choices approximate the displayed Japanese HUD; CHR shapes exact. */
static uint32_t color(unsigned v, int accent)
{
    if (accent && v) return UINT32_C(0xFFFF882C);
    switch (v) {
    case 1u: return UINT32_C(0xFF752617);
    case 2u: return UINT32_C(0xFFFF882C);
    case 3u: return UINT32_C(0xFFF1EEEA);
    default: return UINT32_C(0xFF09070F);
    }
}
static void tile(uint32_t *dst, const unsigned char *chr,
                 unsigned x, unsigned y, unsigned id, int accent,
                 unsigned clip_width)
{
    if (x+8u>CVIII_AUDIO_UI_W || y+8u>CVIII_AUDIO_UI_H) return;
    const unsigned char *src=chr+id*64u;
    for (unsigned iy=0;iy<8u;++iy)
        for (unsigned ix=0;ix<8u && ix<clip_width;++ix) {
            unsigned pixel=src[iy*8u+ix];
            if (pixel) dst[(y+iy)*CVIII_AUDIO_UI_W+x+ix]=color(pixel,accent);
        }
}
static void print(uint32_t *dst,const unsigned char *chr,unsigned x,
                  unsigned y,const char *text,int accent)
{
    for (;*text && x+8u<=CVIII_AUDIO_UI_W;++text,x+=8u) {
        unsigned tile_id=0u;
        if (*text>='A' && *text<='Z')
            tile_id=CVIII_AUDIO_UI_FONT_A+(unsigned)(*text-'A');
        else if (*text>='0' && *text<='9')
            tile_id=CVIII_AUDIO_UI_FONT_ZERO+(unsigned)(*text-'0');
        if (tile_id) tile(dst,chr,x,y,tile_id,accent,8u);
    }
}
static void number(uint32_t *dst,const unsigned char *chr,unsigned y,
                   unsigned value,int accent)
{
    char digits[4];
    digits[0]=(value==100u) ? '1' : ' ';
    digits[1]=(value>=10u) ? (char)('0'+(value/10u)%10u) : ' ';
    digits[2]=(char)('0'+value%10u);
    digits[3]='\0';
    print(dst,chr,VALUE_X,y,digits,accent);
}
static void meter(uint32_t *dst,const unsigned char *chr,
                  unsigned y,unsigned value)
{
    const unsigned filled=(value*BAR_W+50u)/100u;
    for (unsigned i=0;i<BAR_TILES;++i) {
        unsigned x=BAR_X+i*8u;
        tile(dst,chr,x,y,CVIII_AUDIO_UI_BAR_EMPTY,0,8u);
        if (filled>i*8u) {
            const unsigned remaining=filled-i*8u;
            tile(dst,chr,x,y,CVIII_AUDIO_UI_BAR_FULL,0,
                 remaining>=8u ? 8u : remaining);
        }
    }
}
bool cviii_audio_ui_render_mode(uint32_t *pixels,unsigned width,unsigned height,
                                const CviiiChrIndex *assets,
                                const CviiiAudioLevels *volumes,unsigned selected,
                                bool interactive)
{
    if (!pixels || width!=CVIII_AUDIO_UI_W || height!=CVIII_AUDIO_UI_H ||
        !assets || !assets->pixels || !volumes || selected>2u) return false;
    const unsigned char *chr=assets->pixels;
    /* A host-only panel; preserve pixels outside it and all machine state. */
    for (unsigned y=PANEL_Y;y<PANEL_Y+PANEL_H;++y)
        for (unsigned x=PANEL_X;x<PANEL_X+PANEL_W;++x)
            pixels[y*CVIII_AUDIO_UI_W+x]=UINT32_C(0xFF09070F);
    print(pixels,chr,80u,56u,"AUDIO OPTIONS",0);
    static const char *const labels[3]={"MASTER","MUSIC","SFX"};
    const unsigned values[3]={volumes->master,volumes->music,volumes->sfx};
    for(unsigned row=0;row<3u;++row) {
        const unsigned y=ROW_Y+row*ROW_STEP;
        print(pixels,chr,TEXT_X,y,labels[row],row==selected);
        meter(pixels,chr,y,values[row]);
        number(pixels,chr,y,values[row],row==selected);
    }
    if (interactive) {
        /* Both preferences currently persist but do not have independent buses. */
        print(pixels,chr,48u,181u,"MUSIC SFX PENDING",0);
        print(pixels,chr,64u,201u,"F6 ESC CLOSE",0);
    } else {
        print(pixels,chr,56u,181u,"AUDIO PREVIEW",0);
    }
    return true;
}

/* JP0619 preview entrypoint preserved for previously compiled callers. */
bool cviii_audio_ui_render(uint32_t *pixels,unsigned width,unsigned height,
                           const CviiiChrIndex *assets,
                           const CviiiAudioLevels *volumes,unsigned selected)
{
    return cviii_audio_ui_render_mode(pixels,width,height,assets,volumes,
                                      selected,false);
}
