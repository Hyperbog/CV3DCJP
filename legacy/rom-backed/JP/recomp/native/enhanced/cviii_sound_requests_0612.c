/* JP0612 ingress CSV. Does not classify music/SFX or alter host sound. */
#include "cviii_sound_requests_0612.h"
#include <inttypes.h>
#include <string.h>
static CviiiSoundRequests *active;
int cviii_sound_requests_open(CviiiSoundRequests *trace, const char *path)
{
    if (!trace || !path || !*path || active) return 0;
    FILE *f=fopen(path,"w");
    if (!f) return 0;
    memset(trace,0,sizeof(*trace));
    trace->output=f;
    active=trace;
    fputs("cycle,pc,command,x,y,s,p,tracked_prg_bank,stack_next_lo,stack_next_hi\n",f);
    return 1;
}
void cviii_sound_requests_close(CviiiSoundRequests *trace)
{
    if (!trace) return;
    if (active == trace) active=NULL;
    if (trace->output) fclose(trace->output);
    trace->output=NULL;
}
void cviii_sound_requests_observe(uint64_t cycle, uint16_t pc,
    uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p,
    uint8_t tracked_bank, uint8_t stack_lo, uint8_t stack_hi)
{
    CviiiSoundRequests *t=active;
    if (!t || !t->output || pc != 0xE249u) return;
    if (t->requests >= UINT64_C(1000000)) { ++t->dropped; return; }
    fprintf(t->output,"%" PRIu64 ",%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
        cycle,(unsigned)pc,(unsigned)a,(unsigned)x,(unsigned)y,
        (unsigned)s,(unsigned)p,(unsigned)tracked_bank,
        (unsigned)stack_lo,(unsigned)stack_hi);
    ++t->requests;
    if ((t->requests & 127u)==0) fflush(t->output);
}
