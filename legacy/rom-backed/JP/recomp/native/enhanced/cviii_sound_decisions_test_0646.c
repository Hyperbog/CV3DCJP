/* JP0646: real C11 sound decision observer, synthetic machine values only. */
#include "cviii_sound_decisions_0646.h"
/* MSVC Release defines NDEBUG. This integration fixture uses assert to
 * drive events as well as check them, so Release MUST execute assertions. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t ram[2048];
static uint64_t cycle=100u;
static void entry(uint8_t cmd,uint8_t low,uint8_t priority)
{
    ram[0xEF]=cmd;ram[0x109]=low;ram[0x19E]=priority;
    cviii_sound_decisions_fetch(cycle++,0x8852u,0xC9u,ram);
}
static void done(uint16_t pc,uint8_t opcode)
{
    cviii_sound_decisions_fetch(cycle++,pc,opcode,ram);
}
static int hits(const char *line,const char *pattern)
{
    return strstr(line,pattern)!=NULL;
}
int main(void)
{
    CviiiSoundDecisions t={0},t2={0};
    const char *path="jp0646_sound_decisions_synthetic.csv";
    assert(!cviii_sound_decisions_open(NULL,path));
    assert(!cviii_sound_decisions_open(&t,""));
    assert(cviii_sound_decisions_open(&t,path));
    assert(!cviii_sound_decisions_open(&t2,path));
    uint8_t intact[2048];
    cviii_sound_decisions_boundary("power_on",cycle++);
    entry(0x06u,0x50u,0u);
    memcpy(intact,ram,sizeof ram);
    done(0x884Fu,0x60u);
    assert(!memcmp(intact,ram,sizeof ram)); /* no guest RAM mutation */
    entry(0x05u,0x50u,0u);
    done(0x8860u,0x86u);
    entry(0x6Eu,0u,0u);
    done(0x885Du,0x20u);
    done(0x8860u,0x86u);
    entry(0x70u,0u,0x05u);
    ram[0xEF]=1u; /* original driver converts command to DMC index */
    done(0x8922u,0x60u);
    entry(0x74u,0u,0x05u);
    ram[0xEF]=5u;
    done(0x8928u,0x84u);
    assert(t.faults==0 && t.dropped==0);
    /* Invalid PC opcode must not be misreported as a legitimate outcome. */
    entry(0x06u,0x50u,0u);
    done(0x884Fu,0xEAu);
    assert(t.faults==1 && t.pending);
    done(0x8860u,0x86u); /* wrong terminal: classify, don't pretend parity */
    assert(t.faults==2 && !t.pending);
    /* Orphan, nested request, invalid metadata, reset and clock regression. */
    done(0x8922u,0x60u);
    assert(t.faults==3);
    entry(0x6Eu,0u,0u);
    entry(0x70u,0u,0x06u); /* nested */
    assert(t.faults==4);
    done(0x885Du,0x20u); /* invalid metadata on DMC path */
    assert(t.faults==5);
    cviii_sound_decisions_boundary("state_loaded",10u); /* invalid inflight */
    assert(t.faults==6 && !t.pending);
    entry(0x05u,0u,0u);
    done(0x8860u,0x86u);
    assert(t.faults==6); /* load epoch resets monotonicity */
    done(0x8928u,0x84u);
    assert(t.faults==7); /* orphan */
    /* Unsupported boundary labels are ignored. */
    const uint64_t before=t.rows;
    cviii_sound_decisions_boundary("other",1u);
    assert(t.rows==before);
    cviii_sound_decisions_close(&t);
    cviii_sound_decisions_fetch(1u,0x8852u,0xC9u,ram);
    assert(!t.output);
    FILE *f=fopen(path,"r");
    assert(f);
    char line[256]; unsigned rows=0,successful=0,invalid=0,metadata=0;
    assert(fgets(line,sizeof line,f));
    assert(!strcmp(line,"event,id,cycle,pc,command,low_state,priority,counter\n"));
    while(fgets(line,sizeof line,f)) {
        rows++;
        if(hits(line,"invalid_"))invalid++;
        if(hits(line,"meta_install"))metadata++;
        if(hits(line,"dmc_return")||hits(line,"dmc_program")||
           hits(line,"low_return")||hits(line,"low_install"))successful++;
    }
    fclose(f);
    remove(path);
    assert(rows==t.rows && invalid==t.faults);
    assert(successful>=5 && metadata==1 && t.rows>15u);
    puts("PASS JP0646 original sound decision boundaries, all five driver paths, errors and reset");
    return 0;
}
