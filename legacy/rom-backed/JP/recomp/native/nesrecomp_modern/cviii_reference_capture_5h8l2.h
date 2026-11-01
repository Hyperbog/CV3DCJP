#ifndef CVIII_REFERENCE_CAPTURE_5H8L2_H
#define CVIII_REFERENCE_CAPTURE_5H8L2_H
/* Optional, reference-only, one-frame BMP export. Pure C11 and no extra link unit.
 * This does NOT modify the authoritative original 256x240 frame. The host must
 * supply the original framebuffer in 0xAARRGGBB channel order (verify in host).
 * Opt in with CVIII_REF_FRAME_5H8L2=1234 and CVIII_REF_OUT_5H8L2=...bmp.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void cviii_ref_put32_5h8l2(uint8_t *b,uint32_t v){
    for (unsigned n=0;n<4;n++) b[n]=(uint8_t)(v>>(8u*n));
}
static int cviii_ref_write_5h8l2(const char *path,const uint32_t *pixels){
    if(!path || !*path || !pixels) return 0;
    /* Never silently overwrite a prior reference capture. */
    FILE *already=fopen(path,"rb");
    if(already){fclose(already);return 0;}
    FILE *f=fopen(path,"wb");
    if(!f) return 0;
    enum {W=256,H=240,STRIDE=W*3};
    uint8_t hdr[54]={0};memcpy(hdr,"BM",2);
    cviii_ref_put32_5h8l2(hdr+2,54u+STRIDE*H);
    cviii_ref_put32_5h8l2(hdr+10,54);
    cviii_ref_put32_5h8l2(hdr+14,40);
    cviii_ref_put32_5h8l2(hdr+18,W);
    cviii_ref_put32_5h8l2(hdr+22,H);
    cviii_ref_put32_5h8l2(hdr+34,STRIDE*H);
    hdr[26]=1;hdr[28]=24;
    int ok=fwrite(hdr,1,sizeof hdr,f)==sizeof hdr;
    uint8_t row[STRIDE];
    for(int y=H-1;ok && y>=0;y--){
        for(unsigned x=0;x<W;x++){
            uint32_t px=pixels[(size_t)y*W+x];
            row[3*x]=(uint8_t)px;             /* B */
            row[3*x+1]=(uint8_t)(px>>8);     /* G */
            row[3*x+2]=(uint8_t)(px>>16);    /* R */
        }
        ok=fwrite(row,1,STRIDE,f)==STRIDE;
    }
    if(fclose(f)) ok=0;
    if(!ok) remove(path);
    return ok;
}
static void cviii_ref_capture_present_5h8l2(uint64_t frame,const uint32_t *original){
    static int initialized=0,done=0,active=0;
    static uint64_t wanted=0;
    static const char *out=NULL;
    if(!initialized){
        initialized=1;
        const char *p=getenv("CVIII_REF_FRAME_5H8L2");
        out=getenv("CVIII_REF_OUT_5H8L2");
        if(p && *p && out && *out && *p!='-'){
            errno=0;
            char *end=NULL;
            unsigned long long v=strtoull(p,&end,10);
            if(!errno && end && !*end && v>0){wanted=(uint64_t)v;active=1;}
        }
    }
    if(!active || done || !original || frame != wanted) return;
    done=1;
    if(cviii_ref_write_5h8l2(out,original))
        fprintf(stderr,"CVIII 5H8L2: captured original framebuffer at frame %llu -> %s\n",
                (unsigned long long)frame,out);
    else fprintf(stderr,"CVIII 5H8L2: reference export FAILED (path exists or cannot write)\n");
}
#endif
