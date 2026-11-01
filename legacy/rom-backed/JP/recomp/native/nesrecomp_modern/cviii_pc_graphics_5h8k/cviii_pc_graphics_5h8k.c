/* ROM-free native room graphics: actual imported CHR pixels + observed BG banks/
 * palette indices. The 64-color RGB table is an APPROXIMATE display palette;
 * color-emphasis, split-frame CHR changes and sprite layers are not emulated.
 */
#include "cviii_pc_import_5h8i.h"
#include "cviii_pc_world.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 426u
#define H 240u
#define CHR_PIXELS (8192u*64u)
static const uint8_t rom_sha[32]={
  0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
  0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28};
/* Widely used NES-like display palette; not an exact reproduction of a given CRT.
 * Palette RAM indices and graphics pixels, unlike RGB choices, are observed. */
static const uint32_t rgb[64]={
  0x666666,0x002a88,0x1412a7,0x3b00a4,0x5c007e,0x6e0040,0x6c0600,0x561d00,
  0x333500,0x0b4800,0x005200,0x004f08,0x00404d,0x000000,0x000000,0x000000,
  0xadadad,0x155fd9,0x4240ff,0x7527fe,0xa01acc,0xb71e7b,0xb53120,0x994e00,
  0x6b6d00,0x388700,0x0c9300,0x008f32,0x007c8d,0x000000,0x000000,0x000000,
  0xfffeff,0x64b0ff,0x9290ff,0xc676ff,0xf36aff,0xfe6ecc,0xfe8170,0xea9e22,
  0xbcbe00,0x88d800,0x5ce430,0x45e082,0x48cdde,0x4f4f4f,0x000000,0x000000,
  0xfffeff,0xc0dfff,0xd3d2ff,0xe8c8ff,0xfbc2ff,0xfec4ea,0xfeccc5,0xf7d8a5,
  0xe4e594,0xcfef96,0xbdf4ab,0xb3f3cc,0xb5ebf2,0xb8b8b8,0x000000,0x000000
};
static uint16_t u16(const uint8_t *b){return (uint16_t)((unsigned)b[0]|((unsigned)b[1]<<8));}
static uint32_t u32(const uint8_t *b){return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static uint64_t u64(const uint8_t *b){return (uint64_t)u32(b)|((uint64_t)u32(b+4)<<32);}
static uint64_t fnv(const uint8_t *p,size_t len){
    uint64_t v=UINT64_C(0xcbf29ce484222325);
    for(size_t i=0;i<len;i++)v=(v^p[i])*UINT64_C(0x100000001b3);
    return v;
}
static void put32(uint8_t *b,uint32_t v){for(int i=0;i<4;i++)b[i]=(uint8_t)(v>>(8*i));}
static uint8_t *read_all(const char *path,size_t expected){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    uint8_t *p=NULL;
    if(fseek(f,0,SEEK_END)||ftell(f)!=(long)expected||fseek(f,0,SEEK_SET))goto end;
    p=malloc(expected);
    if(!p||fread(p,1,expected,f)!=expected){free(p);p=NULL;}
end:fclose(f);return p;
}
static bool chr_open(const char *path,uint8_t **pixels){
    uint8_t *p=read_all(path,40+CHR_PIXELS);
    if(!p)return false;
    if(memcmp(p,"CVPCCHR1",8)||memcmp(p+8,rom_sha,32))goto fail;
    for(size_t i=40;i<40+CHR_PIXELS;i++)if(p[i]>3)goto fail;
    *pixels=p;return true;
fail:free(p);return false;
}
typedef struct {uint8_t banks[4],bg[16];uint32_t hits,coverage;} Scene;
/* Fail-closed: only a room with a source-observed record can be rendered. */
static bool scene_open(const char *path,uint8_t stage,uint8_t area,uint8_t room,Scene *scene){
    FILE *f=fopen(path,"rb");if(!f)return false;
    uint8_t h[64];bool ok=false;uint8_t *entries=NULL;
    if(fread(h,1,64,f)!=64||memcmp(h,"CVPMAP1\0",8)||u16(h+8)!=1||
       u16(h+10)>135||!u16(h+10)||u32(h+12)!=64u+40u*u16(h+10)||
       memcmp(h+16,rom_sha,32)||u64(h+56)!=0)goto end;
    size_t len=40u*u16(h+10);
    entries=malloc(len);if(!entries||fread(entries,1,len,f)!=len||fgetc(f)!=EOF||
                             u64(h+48)!=fnv(entries,len))goto end;
    uint32_t prev=0;bool first=true;
    for(size_t n=0;n<len;n+=40){
        const uint8_t *q=entries+n;
        uint32_t key=((uint32_t)q[0]<<16)|((uint32_t)q[1]<<8)|q[2];
        if((!first && key<=prev)||q[3]!=1||u32(q+28)>1000||
           u32(q+24)<120||u32(q+32)<u32(q+24)||u32(q+36)!=0)goto corrupt;
        first=false;prev=key;
        for(int j=4;j<8;j++)if(q[j]>=128)goto corrupt;
        for(int j=8;j<24;j++)if(q[j]>63)goto corrupt;
        if(q[0]==stage&&q[1]==area&&q[2]==room){
            memcpy(scene->banks,q+4,4);memcpy(scene->bg,q+8,16);
            scene->hits=u32(q+24);scene->coverage=u32(q+28);ok=true;
        }
    }
    goto end;
corrupt: ok=false;
end:free(entries);fclose(f);return ok;
}
static bool bmp(const char *path,const uint32_t *px){
    const uint32_t stride=(W*3u+3u)&~3u,bytes=stride*H;
    uint8_t head[54]={0};memcpy(head,"BM",2);put32(head+2,54+bytes);
    put32(head+10,54);put32(head+14,40);put32(head+18,W);put32(head+22,H);
    head[26]=1;head[28]=24;put32(head+34,bytes);
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(head,1,54,f)==54;
    uint8_t row[(W*3u+3u)&~3u];
    for(unsigned y=H;ok&&y>0;y--){
        memset(row,0,sizeof(row));
        for(unsigned x=0;x<W;x++){
            uint32_t c=px[(size_t)(y-1)*W+x];
            row[x*3]=(uint8_t)c;row[x*3+1]=(uint8_t)(c>>8);row[x*3+2]=(uint8_t)(c>>16);
        }
        ok=fwrite(row,1,stride,f)==stride;
    }
    if(fclose(f))ok=false;
    return ok;
}
static bool number(const char *s,int64_t *v){
    if(!s||!*s)return false;
    char *end=NULL;errno=0;
    long long x=strtoll(s,&end,10);if(errno||!end||*end)return false;
    *v=(int64_t)x;return true;
}
static bool selector(const char *s,unsigned *st,unsigned *ar,unsigned *rm){
    int n=0;return s&&sscanf(s,"%u/%u/%u%n",st,ar,rm,&n)==3&&s[n]=='\0'&&
                      *st<256&&*ar<256&&*rm<256;
}
static bool render(const CviiiPcRoom *r,const uint8_t *chr,const Scene *s,
                   int64_t camx,int64_t camy,const char *out){
    uint32_t *pixels=calloc((size_t)W*H,sizeof(uint32_t));if(!pixels)return false;
    for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++){
        if(camx < -4096||camx > 65536||camy < -4096||camy > 65536)continue;
        int64_t rx=camx+x,ry=camy+y;
        if(rx<0||ry<0||rx>=(int64_t)r->width*8||ry>=(int64_t)r->height*8)continue;
        uint16_t id=0;
        if(!cviii_pc_get_tile(r,CVIII_PC_TERRAIN,(uint16_t)(rx/8),
                              (uint16_t)(ry/8),&id)||id==0)continue;
        uint32_t encoded=id-1u;
        unsigned pal=(encoded/256u)%4u,pattern=encoded%256u;
        unsigned physical=s->banks[pattern/64u]*64u+(pattern%64u);
        uint8_t ci=chr[40+(size_t)physical*64u+(size_t)(ry%8)*8u+rx%8];
        uint8_t master=s->bg[ci ? pal*4u+ci : 0];
        pixels[(size_t)y*W+x]=rgb[master];
    }
    bool ok=bmp(out,pixels);free(pixels);return ok;
}
static int usage(void){
    fputs("usage: cviii_pc_graphics_5h8k --world PACK --chr CHR --scene-map MAP "
          "--room S/A/R --out BMP [--camera X Y] [--clear X Y] "
          "[--state SAVE] [--save SAVE]\n",stderr);return 2;
}
int main(int argc,char **argv){
    const char *world_path=NULL,*chr_path=NULL,*map_path=NULL,*out=NULL,*state=NULL,*save=NULL;
    unsigned st=0,ar=0,rm=0;int64_t camx=0,camy=0,cx=-1,cy=-1;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--world")&&i+1<argc)world_path=argv[++i];
        else if(!strcmp(argv[i],"--chr")&&i+1<argc)chr_path=argv[++i];
        else if(!strcmp(argv[i],"--scene-map")&&i+1<argc)map_path=argv[++i];
        else if(!strcmp(argv[i],"--out")&&i+1<argc)out=argv[++i];
        else if(!strcmp(argv[i],"--room")&&i+1<argc){if(!selector(argv[++i],&st,&ar,&rm))return usage();}
        else if(!strcmp(argv[i],"--camera")&&i+2<argc){
            if(!number(argv[++i],&camx)||!number(argv[++i],&camy))return usage();}
        else if(!strcmp(argv[i],"--clear")&&i+2<argc){
            if(!number(argv[++i],&cx)||!number(argv[++i],&cy))return usage();}
        else if(!strcmp(argv[i],"--state")&&i+1<argc)state=argv[++i];
        else if(!strcmp(argv[i],"--save")&&i+1<argc)save=argv[++i];
        else return usage();
    }
    if(!world_path||!chr_path||!map_path||!out||(cx<0)!=(cy<0))return usage();
    CviiiPcWorld *w=cviii_pc_world_open_5h8i(world_path);
    if(!w){fputs("ERROR: invalid world pack\n",stderr);return 1;}
    uint8_t *chr=NULL;Scene s={0};int result=1;
    CviiiPcRoom *r=cviii_pc_room(w,(uint8_t)st,(uint8_t)ar,(uint8_t)rm);
    if(!r)fputs("ERROR: room not in native world\n",stderr);
    else if(!chr_open(chr_path,&chr))fputs("ERROR: invalid CHR pack\n",stderr);
    else if(!scene_open(map_path,(uint8_t)st,(uint8_t)ar,(uint8_t)rm,&s))
        fputs("ERROR: no verified room-scene map or corrupt map; refusing guessed graphics\n",stderr);
    else if(state&&!cviii_pc_load(w,state))fputs("ERROR: invalid state file\n",stderr);
    else if(cx>=0&&(cx>=r->width||cy>=r->height||
                     !cviii_pc_change_tile(r,CVIII_PC_TERRAIN,(uint16_t)cx,
                                            (uint16_t)cy,0,CVIII_PC_STAGE)))
        fputs("ERROR: invalid native tile edit\n",stderr);
    else if(!render(r,chr,&s,camx,camy,out)|| (save&&!cviii_pc_save(w,save)))
        fputs("ERROR: failed to render/save\n",stderr);
    else{
        printf("PASS: ROM-free observed-CHR preview %s, room %u/%u/%u; "
               "dominant frames=%" PRIu32 " confidence=%.1f%%\n",out,st,ar,rm,
               s.hits,s.coverage/10.0);
        result=0;
    }
    free(chr);cviii_pc_free(w);return result;
}
