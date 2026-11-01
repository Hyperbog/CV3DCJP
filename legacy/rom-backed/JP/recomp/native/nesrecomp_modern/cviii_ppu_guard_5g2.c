#include "cviii_ppu_guard_5g2.h"

/* Original PPU nametables cover 512x480. Mirroring alone is not new world data. */
static int pixel(const CviiiPpuSource5G2 *s,int y,int dx,uint32_t *argb)
{
    int sx=(s->scroll_x(y)+dx+1024)%512;
    int sy=s->scroll_y(y);
    if(sx<0 || sy<0 || sy>=480)return 0;
    unsigned page=(unsigned)(sx/256)+2u*(unsigned)(sy/240);
    unsigned x=(unsigned)sx%256u, yy=(unsigned)sy%240u;
    uint16_t tile_at=(uint16_t)(0x2000u+page*0x400u+(yy/8u)*32u+x/8u);
    uint16_t attr_at=(uint16_t)(0x23c0u+page*0x400u+(yy/32u)*8u+x/32u);
    unsigned tile=s->nametable(tile_at), attr=s->nametable(attr_at);
    unsigned quadrant=(((yy/16u)&1u)*2u)+((x/16u)&1u);
    unsigned palette=(attr>>(quadrant*2u))&3u;
    unsigned pattern=s->bg_table(y)+tile*16u+(yy&7u);
    unsigned bit=7u-(x&7u);
    unsigned lo=s->chr((uint16_t)pattern);
    unsigned hi=s->chr((uint16_t)(pattern+8u));
    unsigned ci=((lo>>bit)&1u)|(((hi>>bit)&1u)<<1u);
    *argb=s->color(ci?(int)(palette*4u+ci):0);
    return 1;
}
static int sprite_at(const uint8_t *oam,int x,int y)
{
    if(!oam)return 0;
    for(unsigned i=0;i<64u;i++) {
        const uint8_t *o=oam+i*4u;
        if(o[0]>=0xefu)continue;
        int sx=o[3], sy=o[0]+1;
        if(x>=sx && x<sx+8 && y>=sy && y<sy+16)return 1;
    }
    return 0;
}
static int pages_differ(const CviiiPpuSource5G2 *s)
{
    unsigned distinct=0;
    for(unsigned row=5u;row<28u;row+=5u)
        for(unsigned col=3u;col<29u;col+=6u) {
            uint16_t a=(uint16_t)(0x2000u+row*32u+col);
            if(s->nametable(a)!=s->nametable((uint16_t)(a+0x400u)))distinct++;
        }
    return distinct>=4u;
}
static int margin_distinct(const CviiiPpuSource5G2 *s,int origin,int begin,int end)
{
    if(begin>=end)return 0;
    uint8_t seen[32]; unsigned n=0;
    for(int y=64;y<=208;y+=36)
        for(int x=begin+4;x<end;x+=16) {
            int sx=(s->scroll_x(y)+x-origin+1024)%512;
            int sy=s->scroll_y(y);
            if(sx<0 || sy<0 || sy>=480)return 0;
            unsigned page=(unsigned)(sx/256)+2u*(unsigned)(sy/240);
            uint16_t a=(uint16_t)(0x2000u+page*1024u+
                                 ((unsigned)sy%240u/8u)*32u+(unsigned)sx%256u/8u);
            uint8_t t=s->nametable(a);
            unsigned i=0;
            while(i<n && seen[i]!=t)i++;
            if(i==n && n<32)seen[n++]=t;
        }
    return n>=4u;
}
const char *cviii_ppu_result_name_5g2(CviiiPpuResult5G2 r)
{
    switch(r) {
    case CVIII_PPU_OK:return "PPU_VERIFIED_INTERNAL";
    case CVIII_PPU_BAD_INPUT:return "BAD_INPUT";
    case CVIII_PPU_BAD_LINES:return "BAD_LINE_STATE";
    case CVIII_PPU_MIRRORED:return "MIRRORED_NAMETABLE";
    case CVIII_PPU_CENTER_MISMATCH:return "CENTER_MISMATCH";
    case CVIII_PPU_NO_GEOMETRY:return "NO_DISTINCT_GEOMETRY";
    default:return "UNKNOWN";
    }
}
CviiiPpuResult5G2 cviii_ppu_verified_paint_5g2(
    const CviiiPpuSource5G2 *s,const uint32_t *original,
    uint32_t *canvas,int width,int height,int native_x,uint32_t *painted)
{
    if(painted)*painted=0;
    if(!s || !s->nametable || !s->chr || !s->color || !s->scroll_x ||
       !s->scroll_y || !s->bg_table || !s->mask || !original || !canvas ||
       width<=256 || width>864 || height!=240 || native_x<0 ||
       native_x+256>width) return CVIII_PPU_BAD_INPUT;
    for(int y=48;y<240;y++) {
        int sx=s->scroll_x(y), sy=s->scroll_y(y);
        if(!(s->mask(y)&8u) || sx<0 || sx>=512 || sy<0 || sy>=480 ||
           (s->bg_table(y)!=0u && s->bg_table(y)!=0x1000u))
            return CVIII_PPU_BAD_LINES;
    }
    if(!pages_differ(s))return CVIII_PPU_MIRRORED;
    unsigned tested=0,matching=0;
    for(int y=56;y<=216;y+=16)
        for(int x=16;x<=240;x+=16) {
            if(sprite_at(s->oam,x,y))continue;
            uint32_t expected;
            if(!pixel(s,y,x,&expected))return CVIII_PPU_BAD_LINES;
            tested++;
            if(original[(size_t)y*256u+(unsigned)x]==expected)matching++;
        }
    if(tested<90u || matching*100u<tested*96u)
        return CVIII_PPU_CENTER_MISMATCH;
    int left=margin_distinct(s,native_x,0,native_x);
    int right=margin_distinct(s,native_x,native_x+256,width);
    if(!left && !right)return CVIII_PPU_NO_GEOMETRY;
    /* Validate all input coordinates before beginning any framebuffer write. */
    uint32_t n=0;
    for(int y=48;y<height;y++) {
        if(left)for(int x=0;x<native_x;x++) {
            uint32_t c;
            if(!pixel(s,y,x-native_x,&c))return CVIII_PPU_BAD_LINES;
            canvas[(size_t)y*(unsigned)width+(unsigned)x]=c;n++;
        }
        if(right)for(int x=native_x+256;x<width;x++) {
            uint32_t c;
            if(!pixel(s,y,x-native_x,&c))return CVIII_PPU_BAD_LINES;
            canvas[(size_t)y*(unsigned)width+(unsigned)x]=c;n++;
        }
    }
    if(painted)*painted=n;
    return CVIII_PPU_OK;
}
