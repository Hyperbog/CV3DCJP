#include "cviii_diagnostic_5h8.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
static long bytes(const char *suffix) {
    char path[180];snprintf(path,sizeof path,"cviii_diag_test%s",suffix);
    FILE *f=fopen(path,"rb");assert(f);
    assert(fseek(f,0,SEEK_END)==0);long n=ftell(f);fclose(f);
    remove(path);return n;
}
int main(void) {
    uint8_t ram[2048]={0},oam[256]={0},pal[32]={0},lines[1200]={0},
            chr[8192]={0},ciram[2048]={0};
    ram[0x34]=0;ram[0x35]=1;ram[0x36]=0;
    CviiiDiagnosticFrame5H8 f={0};
    f.frame=10;f.cycles=1000;f.pc=0x8110;f.a=17;
    f.ram=ram;f.oam=oam;f.palette=pal;f.lines=lines;
    f.chr=chr;f.ciram=ciram;f.ciram_len=sizeof ciram;
    CviiiDiagnostic5H8 d={0};
    assert(cviii_diag_open_5h8(&d,"cviii_diag_test",1));
    assert(cviii_diag_append_5h8(&d,&f));
    assert(!cviii_diag_append_5h8(&d,&f)); /* duplicates rejected */
    f.frame=12;f.cycles+=300;oam[10]=1;chr[1025]=7;
    assert(cviii_diag_append_5h8(&d,&f));
    cviii_diag_close_5h8(&d);
    assert(bytes(".oam.bin")==2*(8+256));
    assert(bytes(".lines.bin")==2*(8+1200));
    assert(bytes(".palette.bin")==2*(8+32));
    assert(bytes(".ram.bin")==2*(8+2048));
    assert(bytes(".ciram.bin")==2*(8+4+2048));
    assert(bytes(".chr-changes.bin")==2*(8+8192));
    assert(bytes(".frames.csv")>200);
    assert(bytes(".events.csv")>65);
    puts("PASS: CPU, OAM, PPU, RAM, CHR capture + gap/change events");
    return 0;
}
