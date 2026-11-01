/* Invented nametables + pixels, no original game code/ROM data. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_ppu_guard_5g2.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { W=426,H=240,O=85 };
static uint8_t nt[4096],oam[256];
static uint32_t native[256*H],wide[W*H],prior[W*H];
static int invalid_line=-1,wrong_scroll=0;
static uint8_t nametable(uint16_t address){return nt[address&4095u];}
static uint8_t chr(uint16_t address){return (address&15u)<8u?255u:0u;}
static uint32_t color(int entry){return 0xff000000u|(unsigned)entry;}
static int scroll_x(int y){(void)y;return wrong_scroll?-1:96;}
static int scroll_y(int y){return y%240;}
static uint16_t table(int y){(void)y;return 0x1000;}
static uint8_t mask(int y){return y==invalid_line?0:0x1e;}
static CviiiPpuSource5G2 s={nametable,chr,color,scroll_x,scroll_y,table,mask,oam};
static void fixture(void)
{
    memset(oam,255,sizeof(oam));
    for(int p=0;p<4;p++) {
        for(unsigned y=0;y<30;y++)for(unsigned x=0;x<32;x++)
            nt[p*1024+y*32+x]=(uint8_t)(p*41u+x+y*7u+1u);
        memset(nt+p*1024+960,0,64);
    }
    for(unsigned i=0;i<256*H;i++)native[i]=color(1);
    for(unsigned i=0;i<W*H;i++)wide[i]=0xff232323u;
    for(int y=0;y<H;y++)memcpy(wide+y*W+O,native+y*256,256*sizeof(uint32_t));
    memcpy(prior,wide,sizeof(prior));
}
int main(void)
{
    fixture();uint32_t painted=0;
    assert(cviii_ppu_verified_paint_5g2(&s,native,wide,W,H,O,&painted)==CVIII_PPU_OK);
    assert(painted==192u*170u);
    for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++) {
        size_t i=(size_t)y*W+x;
        if(x>=O && x<O+256)assert(wide[i]==prior[i]);
        else if(y<48)assert(wide[i]==0xff232323u);
        else assert(wide[i]==color(1));
    }
    fixture();wrong_scroll=1;
    assert(cviii_ppu_verified_paint_5g2(&s,native,wide,W,H,O,&painted)==CVIII_PPU_BAD_LINES);
    assert(painted==0 && memcmp(wide,prior,sizeof(wide))==0);wrong_scroll=0;
    fixture();memcpy(nt+1024,nt,1024);memcpy(nt+3072,nt+2048,1024);
    assert(cviii_ppu_verified_paint_5g2(&s,native,wide,W,H,O,&painted)==CVIII_PPU_MIRRORED);
    assert(memcmp(wide,prior,sizeof(wide))==0);
    fixture();for(unsigned y=56;y<=216;y+=16)for(unsigned x=16;x<=240;x+=16)
        native[y*256+x]=0xff192837u;
    assert(cviii_ppu_verified_paint_5g2(&s,native,wide,W,H,O,&painted)==CVIII_PPU_CENTER_MISMATCH);
    assert(memcmp(wide,prior,sizeof(wide))==0);
    fixture();invalid_line=90;
    assert(cviii_ppu_verified_paint_5g2(&s,native,wide,W,H,O,&painted)==CVIII_PPU_BAD_LINES);
    assert(memcmp(wide,prior,sizeof(wide))==0);invalid_line=-1;
    assert(cviii_ppu_result_name_5g2(CVIII_PPU_OK)[0]=='P');
    puts("PASS: guarded PPU extension, mirrored pages, visible parity, no writes on rejection");
    return 0;
}
