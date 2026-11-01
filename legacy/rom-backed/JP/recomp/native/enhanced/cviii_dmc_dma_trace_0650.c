/* JP0650: actual mapper/DMC DMA source evidence from pinned private overlays.
 * Never reread a hardware address, install a mapper register, or emulate DMA.
 */
#include "cviii_dmc_dma_trace_0650.h"
#include <inttypes.h>
#include <string.h>
static CviiiDmcDmaTrace *active;
enum { ROW_LIMIT=200000 };
static void record(CviiiDmcDmaTrace *t, const char *kind,uint64_t cycle,
                   int addr,int64_t physical,int value,int bank8,
                   int bytes_before,int requested)
{
    if(t->rows>=ROW_LIMIT){
        ++t->dropped;
        /* A truncated owner trace must NEVER pass as a complete capture.
         * Emit exactly one terminal row; the strict CSV verifier rejects it.
         * Do not perform unbounded I/O after the cap has been reached. */
        if(t->dropped==1u){
            (void)fprintf(t->output,
                "trace_limit,%" PRIu32 ",%" PRIu64 ",-1,-1,-1,-1,-1,-1\n",
                t->epoch,cycle);
            fflush(t->output);
        }
        return;
    }
    if(fprintf(t->output,
        "%s,%" PRIu32 ",%" PRIu64 ",%d,%" PRId64 ",%d,%d,%d,%d\n",
        kind,t->epoch,cycle,addr,physical,value,bank8,
        bytes_before,requested)<0)++t->faults;
    ++t->rows;
    if((t->rows&127u)==0u)fflush(t->output);
}
static int valid_clock(CviiiDmcDmaTrace *t,uint64_t cycle)
{
    const int ok=!t->seen_cycle || cycle>=t->last_cycle;
    if(!ok)++t->faults;
    t->last_cycle=cycle;t->seen_cycle=1u;
    return ok;
}
int cviii_dmc_dma_trace_open(CviiiDmcDmaTrace *t,const char *path)
{
    if(!t || !path || !*path || active)return 0;
    FILE *f=fopen(path,"w");
    if(!f)return 0;
    memset(t,0,sizeof(*t));
    t->output=f;
    active=t;
    if(fputs("event,epoch,cycle,cpu_addr,physical,byte,bank8,bytes_before,requested_bank8\n",f)<0){
        cviii_dmc_dma_trace_close(t);return 0;
    }
    return 1;
}
void cviii_dmc_dma_trace_close(CviiiDmcDmaTrace *t)
{
    if(!t)return;
    if(active==t)active=NULL;
    if(t->output)fclose(t->output);
    t->output=NULL;
}
void cviii_dmc_dma_trace_boundary(const char *kind,uint64_t cycle)
{
    CviiiDmcDmaTrace *t=active;
    if(!t || !t->output || !kind)return;
    if(strcmp(kind,"power_on") && strcmp(kind,"state_loaded"))return;
    ++t->epoch;
    t->seen_cycle=0;
    record(t,kind,cycle,-1,-1,-1,-1,-1,-1);
    fflush(t->output);
}
void cviii_dmc_dma_trace_bank_write(uint64_t cycle,uint16_t addr,uint8_t value)
{
    CviiiDmcDmaTrace *t=active;
    if(!t || !t->output)return;
    const int correct=(addr&0xF000u)==0xC000u;
    const char *name=correct?"bank_request":"invalid_bank_addr";
    if(!valid_clock(t,cycle))name="invalid_cycle";
    if(!correct)++t->faults;
    record(t,name,cycle,(int)addr,-1,-1,-1,-1,(int)(value&31u));
    ++t->bank_requests;
}
void cviii_dmc_dma_trace_fetch(uint64_t cycle,uint16_t addr,uint32_t physical,
                               uint8_t value,unsigned bytes_before)
{
    CviiiDmcDmaTrace *t=active;
    if(!t || !t->output)return;
    const int is_prg=addr>=0x8000u;
    const int physical_ok=physical<262144u;
    const int cbank=addr>=0xC000u && addr<0xE000u;
    const int bank8=cbank && physical_ok?(int)(physical/8192u):-1;
    const uint32_t in_bank=physical%8192u;
    const int aligned=!cbank || in_bank==(uint32_t)(addr-0xC000u);
    const int correct=is_prg && physical_ok && aligned && bytes_before<=65535u;
    const char *name=correct?"dma_fetch":"invalid_fetch_map";
    if(!valid_clock(t,cycle))name="invalid_cycle";
    if(!correct)++t->faults;
    record(t,name,cycle,(int)addr,(int64_t)physical,(int)value,bank8,
           bytes_before<=65535u?(int)bytes_before:-1,-1);
    ++t->fetches;
}
