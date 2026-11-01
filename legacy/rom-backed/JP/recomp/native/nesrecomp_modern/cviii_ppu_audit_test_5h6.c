/* All synthetic, no Japanese ROM or NESRecomp required. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_ppu_audit_5h6.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int shifted;
static int sx(int y){return y>=120?9:9;}
static int sy(int y){return (y+shifted*(y>=120))%480;}
static uint16_t bg(int y){return y==119&&shifted?0:0x1000;}
static uint8_t mask(int y){return y==119&&shifted?0x08:0x1e;}
int main(void)
{
    CviiiPpuProbe5H6 src={sx,sy,bg,mask};
    CviiiPpuSummary5H6 result={0};
    assert(cviii_ppu_probe_5h6(&src,&result));
    assert(result.y119==119&&result.y120==120&&result.vertical_phase_breaks==0);
    assert(result.pattern_changes==0&&result.mask_changes==0);
    shifted=1;
    assert(cviii_ppu_probe_5h6(&src,&result));
    assert(result.y119==119&&result.y120==121&&result.vertical_phase_breaks==1);
    assert(result.pattern_changes==2&&result.mask_changes==2);
    unsigned tops[8]={0};
    assert(cviii_ppu_camera_tops_5h6(0,1,0,119,tops)==8);
    assert(tops[0]==48&&tops[1]==32&&tops[2]==0&&tops[3]==47);
    assert(cviii_ppu_camera_tops_5h6(0,0,0,119,tops)==3);
    assert(cviii_ppu_camera_tops_5h6(0,1,0,120,tops)==3);
    FILE *f=tmpfile();assert(f);
    CviiiPpuAudit5H6 trace={0};trace.file=f;
    uint8_t banks[]={2,3,4,5};
    assert(cviii_ppu_append_5h6(&trace,77,0,1,0,5,0,&result,banks,true));
    assert(!cviii_ppu_append_5h6(&trace,77,0,1,0,5,0,&result,banks,true));
    assert(cviii_ppu_append_5h6(&trace,78,0,0,0,5,1,&result,NULL,false));
    rewind(f);char line[512];assert(fgets(line,sizeof line,f));
    assert(strstr(line,"77,0,1,0,5,0,")==line);
    assert(strstr(line,",1,2,3,4,5")!=NULL); /* frame-end CHR provenance */
    assert(fgets(line,sizeof line,f));
    assert(strstr(line,"78,0,0,0,5,1,")==line);
    assert(strstr(line,",0,-1,-1,-1,-1")!=NULL);
    cviii_ppu_close_5h6(&trace);
    puts("PASS: per-line PPU captures; 119/120 split; safe candidate restriction; CSV provenance");
    return 0;
}
