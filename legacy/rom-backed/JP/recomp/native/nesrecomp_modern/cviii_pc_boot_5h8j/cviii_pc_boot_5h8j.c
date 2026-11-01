/* PC-native resource viewer. No original ROM, NES CPU/PPU, or SDL dependency.
 * The room view is an indexed-tile DEBUG VISUALIZATION, not authentic game art.
 * The CHR atlas shows actual imported grayscale 2-bpp physical tile pixels.
 */
#include "cviii_pc_import_5h8i.h"
#include "cviii_pc_world.h"
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_W 426u
#define VIEW_H 240u
#define CHR_BYTES (8192u*64u)
#define CHR_HEADER 40u
static const uint8_t rom_sha[32]={
  0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
  0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28
};
static bool number(const char *s,int64_t *v) {
    char *end=NULL;
    if(!s||!*s) return false;
    errno=0;
    long long n=strtoll(s,&end,10);
    if(errno || !end || *end) return false;
    *v=(int64_t)n;return true;
}
static bool selector(const char *s,unsigned *st,unsigned *ar,unsigned *rm) {
    int tail=0;
    return s && sscanf(s,"%u/%u/%u%n",st,ar,rm,&tail)==3 && s[tail]=='\0' &&
           *st<256 && *ar<256 && *rm<256;
}
static void le16(uint8_t *b,unsigned n){ b[0]=(uint8_t)n;b[1]=(uint8_t)(n>>8); }
static void le32(uint8_t *b,uint32_t n){ for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(n>>(8*i)); }
/* Portable 24-bit bottom-up uncompressed BMP (requires width*3+3 fitting). */
static bool bmp_write(const char *path,const uint32_t *rgba,unsigned w,unsigned h) {
    if(!path||!rgba||!w||!h||w>4096||h>4096)return false;
    uint32_t stride=(w*3u+3u)&~3u,bytes=stride*h;
    uint8_t header[54]={0};
    memcpy(header,"BM",2);le32(header+2,54+bytes);le32(header+10,54);
    le32(header+14,40);le32(header+18,w);le32(header+22,h);
    le16(header+26,1);le16(header+28,24);le32(header+34,bytes);
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(header,1,sizeof(header),f)==sizeof(header);
    uint8_t *line=(uint8_t*)calloc(1,stride);
    if(!line)ok=false;
    for(unsigned y=0;ok && y<h;y++) {
        const uint32_t *pixels=rgba+(size_t)(h-1-y)*w;
        for(unsigned x=0;x<w;x++){
            uint32_t c=pixels[x];line[x*3]=(uint8_t)c;
            line[x*3+1]=(uint8_t)(c>>8);line[x*3+2]=(uint8_t)(c>>16);
        }
        ok=fwrite(line,1,stride,f)==stride;
    }
    free(line);
    if(fclose(f))ok=false;
    return ok;
}
/* Distinct visual IDs only: never invent an actual Japanese ROM palette. */
static uint32_t diagnostic_color(uint16_t tile,unsigned x,unsigned y){
    if(!tile)return 0xff000000u;
    unsigned palette=((tile-1)/256u)%4u;
    unsigned pattern=(tile-1)%256u;
    unsigned group=(tile-1)/1024u;
    uint32_t r=45u+(pattern*7u+group*17u)%120u;
    uint32_t g=42u+(palette*47u+pattern*3u)%130u;
    uint32_t b=50u+(group*13u+pattern*5u)%120u;
    if(x==0||y==0){r/=2;g/=2;b/=2;}
    return 0xff000000u|(r<<16)|(g<<8)|b;
}
static bool render_room(const CviiiPcRoom *room,int64_t camera_x,int64_t camera_y,
                        const char *out) {
    if(!room)return false;
    uint32_t *picture=(uint32_t*)malloc((size_t)VIEW_W*VIEW_H*sizeof(uint32_t));
    if(!picture)return false;
    for(unsigned py=0;py<VIEW_H;py++)for(unsigned px=0;px<VIEW_W;px++){
        /* Avoid signed overflow when callers provide extreme positions. */
        uint32_t c=0xff000000u;
        if(camera_x>=-4096 && camera_x<=65536 && camera_y>=-4096 && camera_y<=65536) {
            int64_t rx=camera_x+(int64_t)px,ry=camera_y+(int64_t)py;
            if(rx>=0 && ry>=0 && rx<(int64_t)room->width*8 && ry<(int64_t)room->height*8){
                uint16_t tile=0;
                if(cviii_pc_get_tile(room,CVIII_PC_TERRAIN,(uint16_t)(rx/8),
                                     (uint16_t)(ry/8),&tile))
                    c=diagnostic_color(tile,(unsigned)rx%8,(unsigned)ry%8);
            }
        }
        picture[(size_t)py*VIEW_W+px]=c;
    }
    bool ok=bmp_write(out,picture,VIEW_W,VIEW_H);
    free(picture);return ok;
}
static uint8_t *read_chr(const char *filename) {
    FILE *f=fopen(filename,"rb");if(!f)return NULL;
    uint8_t *buf=NULL;
    if(fseek(f,0,SEEK_END))goto done;
    long n=ftell(f);
    if(n!=(long)(CHR_BYTES+CHR_HEADER)||fseek(f,0,SEEK_SET))goto done;
    buf=(uint8_t*)malloc(CHR_BYTES+CHR_HEADER);
    if(!buf||fread(buf,1,CHR_BYTES+CHR_HEADER,f)!=CHR_BYTES+CHR_HEADER||
       memcmp(buf,"CVPCCHR1",8)||memcmp(buf+8,rom_sha,32))goto fail;
    for(size_t k=CHR_HEADER;k<CHR_BYTES+CHR_HEADER;k++)if(buf[k]>3)goto fail;
    goto done;
fail: free(buf);buf=NULL;
done: fclose(f);return buf;
}
static bool render_chr(const uint8_t *chr,const char *out) {
    if(!chr)return false;
    const unsigned w=512,h=1024;
    uint32_t *pix=(uint32_t*)malloc((size_t)w*h*sizeof(uint32_t));
    if(!pix)return false;
    const uint32_t shades[4]={0xff000000u,0xff555555u,0xffaaaaaau,0xffffffffu};
    for(unsigned t=0;t<8192;t++)for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)
        pix[((size_t)(t/64)*8+y)*w+(t%64)*8+x]=shades[chr[CHR_HEADER+(size_t)t*64+y*8+x]];
    bool ok=bmp_write(out,pix,w,h);free(pix);return ok;
}
static int usage(void){
    fprintf(stderr,"Usage: cviii_pc_boot_5h8j --world WORLD --chr CHR --out BMP "
           "[--room S/A/R] [--camera X Y] [--clear TILE_X TILE_Y] "
           "[--state LOAD] [--save SAVE] [--atlas] [--list]\n");return 2;
}
int main(int argc,char **argv){
    const char *world_path=NULL,*chr_path=NULL,*out=NULL,*load=NULL,*save=NULL;
    unsigned s=0,a=0,r=0;int64_t cam_x=0,cam_y=0;
    int64_t clear_x=-1,clear_y=-1;bool atlas=false,list=false;
    for(int i=1;i<argc;i++) {
        const char *arg=argv[i];
        if(!strcmp(arg,"--world") && i+1<argc)world_path=argv[++i];
        else if(!strcmp(arg,"--chr") && i+1<argc)chr_path=argv[++i];
        else if(!strcmp(arg,"--out") && i+1<argc)out=argv[++i];
        else if(!strcmp(arg,"--state") && i+1<argc)load=argv[++i];
        else if(!strcmp(arg,"--save") && i+1<argc)save=argv[++i];
        else if(!strcmp(arg,"--room") && i+1<argc){if(!selector(argv[++i],&s,&a,&r))return usage();}
        else if(!strcmp(arg,"--camera") && i+2<argc) {
            if(!number(argv[++i],&cam_x)||!number(argv[++i],&cam_y))return usage();
        }else if(!strcmp(arg,"--clear") && i+2<argc) {
            if(!number(argv[++i],&clear_x)||!number(argv[++i],&clear_y))return usage();
        }else if(!strcmp(arg,"--atlas"))atlas=true;
        else if(!strcmp(arg,"--list"))list=true;
        else return usage();
    }
    if(!world_path||!chr_path||!out)return usage();
    CviiiPcWorld *world=cviii_pc_world_open_5h8i(world_path);
    if(!world){fprintf(stderr,"ERROR: invalid or absent world pack\n");return 1;}
    uint8_t *chr=read_chr(chr_path);
    if(!chr){fprintf(stderr,"ERROR: invalid or absent CHR catalogue\n");cviii_pc_free(world);return 1;}
    int result=1;
    if(load && !cviii_pc_load(world,load))fprintf(stderr,"ERROR: native save rejected\n");
    else {
        if(list)for(size_t i=0;i<world->room_count;i++){
            const CviiiPcRoom *q=&world->rooms[i];
            printf("room %u/%u/%u %ux%u tiles placed=%d\n",
                   q->stage,q->area,q->room,q->width,q->height,(int)q->placed);
        }
        CviiiPcRoom *room=cviii_pc_room(world,(uint8_t)s,(uint8_t)a,(uint8_t)r);
        if(!atlas && !room)fprintf(stderr,"ERROR: room selector is absent\n");
        else if(!atlas && clear_x>=0 && clear_y>=0 &&
                 (clear_x>=room->width || clear_y>=room->height ||
                  !cviii_pc_change_tile(room,CVIII_PC_TERRAIN,(uint16_t)clear_x,
                                        (uint16_t)clear_y,0,CVIII_PC_STAGE)))
            fprintf(stderr,"ERROR: tile edit outside selected room\n");
        else if((atlas ? render_chr(chr,out) : render_room(room,cam_x,cam_y,out)) &&
                (!save || cviii_pc_save(world,save))) {
            printf("PASS: ROM-free native %s rendered %s\n",atlas?"physical CHR atlas":"room tile-ID diagnostic",out);
            printf("world rooms=%zu entities=%zu signature=%016" PRIx64 "\n",
                   world->room_count,world->entity_count,world->resource_signature);
            result=0;
        }else fprintf(stderr,"ERROR: output or save operation failed\n");
    }
    free(chr);cviii_pc_free(world);return result;
}
