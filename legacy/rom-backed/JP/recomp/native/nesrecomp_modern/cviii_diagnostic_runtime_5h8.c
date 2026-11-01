#include "cviii_diagnostic_5h8.h"
#include "cyc_core.h"
#include "cyc_render.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static CviiiDiagnostic5H8 record;
static char path[1024];
static int enabled,full,initialized,failed;
static void close_at_exit(void){cviii_diag_close_5h8(&record);}
int cviii_diag_runtime_open_5h8(const char *p) {
    if(!p||!*p||strlen(p)>=sizeof path||initialized)return 0;
    strcpy(path,p);enabled=1;return 1;
}
int cviii_diag_runtime_mode_5h8(const char *p) {
    if(!p||initialized)return 0;
    if(!strcmp(p,"off")){enabled=0;return 1;}
    if(!strcmp(p,"frames")){enabled=1;full=0;return 1;}
    if(!strcmp(p,"full")){enabled=1;full=1;return 1;}
    return 0;
}
void cviii_diag_runtime_frame_5h8(uint64_t id) {
    if(!enabled||failed||!path[0])return;
    if(!initialized){
        initialized=1;
        if(!cviii_diag_open_5h8(&record,path,full)){
            fprintf(stderr,"CVIII-5H8: cannot open capture prefix: %s\n",path);
            failed=1;return;
        }
        atexit(close_at_exit);
    }
    const uint8_t *ram=cyc_cpu_ram(),*oam=cyc_render_oam();
    const CycLine *lines=cyc_frame_lines();
    size_t ciram_len=0;
    const uint8_t *ciram=cyc_ppu_ciram(&ciram_len),*pal=cyc_ppu_palette();
    if(!ram||!oam||!lines||!pal)return;
    uint8_t packed[CVIII_DIAG_LINES*CVIII_DIAG_LINE_BYTES],chr[CVIII_DIAG_CHR_BYTES];
    for(unsigned i=0;i<240u;i++) {
        packed[5u*i]=(uint8_t)lines[i].v;
        packed[5u*i+1]=(uint8_t)(lines[i].v>>8);
        packed[5u*i+2]=lines[i].fine_x;
        packed[5u*i+3]=lines[i].ctrl;
        packed[5u*i+4]=lines[i].mask;
    }
    for(unsigned i=0;i<CVIII_DIAG_CHR_BYTES;i++)chr[i]=cyc_render_chr((uint16_t)i);
    CycCpuState cpu={0};cyc_cpu_state(&cpu);
    CviiiDiagnosticFrame5H8 f={0};
    f.frame=id;f.cycles=cyc_cycle_count();f.pc=cpu.pc;
    f.a=cpu.a;f.x=cpu.x;f.y=cpu.y;f.s=cpu.s;f.p=cpu.p;
    f.nmi=cpu.do_nmi;f.irq=cpu.do_irq;
    f.ram=ram;f.oam=oam;f.palette=pal;f.ciram=ciram;f.ciram_len=ciram_len;
    f.lines=packed;f.chr=chr;
    if(!cviii_diag_append_5h8(&record,&f)){
        fprintf(stderr,"CVIII-5H8: capture write failed at frame %llu\n",
                (unsigned long long)id);
        failed=1;cviii_diag_close_5h8(&record);
    }
}
