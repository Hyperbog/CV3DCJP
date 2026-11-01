#include "cviii_diagnostic_5h8.h"
#include <inttypes.h>
#include <string.h>
/* FNV-1a is a change detector, NOT authentication. */
static uint32_t digest(const uint8_t *p,size_t n) {
    uint32_t h=UINT32_C(2166136261);
    for(size_t i=0;p && i<n;i++)h=(h^p[i])*UINT32_C(16777619);
    return p?h:0;
}
static int put16(FILE *f,uint16_t n) {
    unsigned char a[2]={(unsigned char)n,(unsigned char)(n>>8)};
    return fwrite(a,1,2,f)==2;
}
static int put32(FILE *f,uint32_t n) {
    return put16(f,(uint16_t)n) && put16(f,(uint16_t)(n>>16));
}
static int put64(FILE *f,uint64_t n) {
    return put32(f,(uint32_t)n)&&put32(f,(uint32_t)(n>>32));
}
static int binary(FILE *f,uint64_t frame,const uint8_t *p,size_t n) {
    return f && p && put64(f,frame) && fwrite(p,1,n,f)==n;
}
static FILE *sidecar(const char *p,const char *suffix,const char *mode) {
    char path[1200];size_t a=strlen(p),b=strlen(suffix);
    if(a+b+1>=sizeof path)return NULL;
    memcpy(path,p,a);memcpy(path+a,suffix,b+1);
    return fopen(path,mode);
}
void cviii_diag_close_5h8(CviiiDiagnostic5H8 *d) {
    if(!d)return;
    FILE *files[]={d->frames,d->oam,d->lines,d->palette,d->ram,
                   d->ciram,d->chr,d->events};
    for(unsigned i=0;i<8;i++)if(files[i])fclose(files[i]);
    d->frames=d->oam=d->lines=d->palette=d->ram=d->ciram=d->chr=d->events=NULL;
    d->seen=0;
}
int cviii_diag_open_5h8(CviiiDiagnostic5H8 *d,const char *p,int full) {
    if(!d||!p||!*p||strlen(p)>1000)return 0;
    memset(d,0,sizeof *d);d->full=full!=0;
    d->frames=sidecar(p,".frames.csv","wb");
    d->oam=sidecar(p,".oam.bin","wb");
    d->lines=sidecar(p,".lines.bin","wb");
    d->palette=sidecar(p,".palette.bin","wb");
    d->events=sidecar(p,".events.csv","wb");
    if(d->full){
        d->ram=sidecar(p,".ram.bin","wb");
        d->ciram=sidecar(p,".ciram.bin","wb");
        d->chr=sidecar(p,".chr-changes.bin","wb");
    }
    if(!d->frames||!d->oam||!d->lines||!d->palette||!d->events||
       (d->full&&(!d->ram||!d->ciram||!d->chr))) {
        cviii_diag_close_5h8(d);return 0;
    }
    fputs("frame,cycles,pc,a,x,y,s,p,nmi,irq,stage,area,room,ram_56_57,"
          "ram_fnv,ciram_fnv,palette_fnv,oam_fnv,lines_fnv,chr_fnv,"
          "chr_0,chr_1,chr_2,chr_3,chr_4,chr_5,chr_6,chr_7\n",d->frames);
    fputs("frame,event,detail\n",d->events);
    return 1;
}
static int event(CviiiDiagnostic5H8 *d,uint64_t frame,
                 const char *type,const char *detail) {
    return fprintf(d->events,"%" PRIu64 ",%s,%s\n",frame,type,detail)>0;
}
int cviii_diag_append_5h8(CviiiDiagnostic5H8 *d,const CviiiDiagnosticFrame5H8 *f) {
    if(!d||!f||!d->frames||!f->ram||!f->oam||!f->palette||
       !f->lines||!f->chr||f->ciram_len>65536||
       (f->ciram_len&&!f->ciram)||(d->seen&&f->frame<=d->last_frame))return 0;
    const uint8_t stage=f->ram[0x34],area=f->ram[0x35],room=f->ram[0x36];
    uint32_t hram=digest(f->ram,CVIII_DIAG_RAM_BYTES),
             hciram=digest(f->ciram,f->ciram_len),
             hpal=digest(f->palette,32),hoam=digest(f->oam,256),
             hline=digest(f->lines,CVIII_DIAG_LINES*CVIII_DIAG_LINE_BYTES),
             hchr=digest(f->chr,CVIII_DIAG_CHR_BYTES);
    if(fprintf(d->frames,"%" PRIu64 ",%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,"
              "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
       f->frame,f->cycles,(unsigned)f->pc,f->a,f->x,f->y,f->s,f->p,
       f->nmi,f->irq,stage,area,room,
       (unsigned)f->ram[0x56]|((unsigned)f->ram[0x57]<<8),
       hram,hciram,hpal,hoam,hline,hchr)<0)return 0;
    for(unsigned k=0;k<8;k++) {
        if(fprintf(d->frames,",%u",digest(f->chr+k*1024u,1024u))<0)return 0;
    }
    if(fputc('\n',d->frames)==EOF)return 0;
    if(!binary(d->oam,f->frame,f->oam,256) ||
       !binary(d->lines,f->frame,f->lines,CVIII_DIAG_LINES*CVIII_DIAG_LINE_BYTES) ||
       !binary(d->palette,f->frame,f->palette,32))return 0;
    if(d->full) {
        if(!binary(d->ram,f->frame,f->ram,CVIII_DIAG_RAM_BYTES) ||
           !put64(d->ciram,f->frame)||!put32(d->ciram,(uint32_t)f->ciram_len)||
           (f->ciram_len&&fwrite(f->ciram,1,f->ciram_len,d->ciram)!=f->ciram_len))return 0;
        if(!d->seen||hchr!=d->prior_chr)
            if(!binary(d->chr,f->frame,f->chr,CVIII_DIAG_CHR_BYTES))return 0;
    }
    if(!d->seen||stage!=d->stage||area!=d->area||room!=d->room) {
        char detail[50];snprintf(detail,sizeof detail,"%u/%u/%u",stage,area,room);
        if(!event(d,f->frame,"ROOM",detail))return 0;
    }
    if(d->seen && f->frame!=d->last_frame+1u)
        if(!event(d,f->frame,"FRAME_GAP","nonconsecutive"))return 0;
    if(d->seen && hchr!=d->prior_chr)
        if(!event(d,f->frame,"CHR_CHANGED","mapped_8kb"))return 0;
    if(d->seen && hoam!=d->prior_oam)
        if(!event(d,f->frame,"OAM_CHANGED","256_bytes"))return 0;
    if(d->seen && hline!=d->prior_lines)
        if(!event(d,f->frame,"SCANLINES_CHANGED","240_lines"))return 0;
    d->stage=stage;d->area=area;d->room=room;
    d->last_frame=f->frame;d->prior_chr=hchr;d->prior_oam=hoam;
    d->prior_lines=hline;d->seen=1;
    if(++d->flush_count>=120u) {
        FILE *files[]={d->frames,d->oam,d->lines,d->palette,d->ram,
                       d->ciram,d->chr,d->events};
        for(unsigned i=0;i<8;i++)if(files[i])fflush(files[i]);
        d->flush_count=0;
    }
    return 1;
}
