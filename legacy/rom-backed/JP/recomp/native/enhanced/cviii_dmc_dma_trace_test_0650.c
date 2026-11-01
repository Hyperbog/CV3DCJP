/* JP0650: production read-only DMA logger tests; synthetic no ROM.
 * Assert-driven fixtures must run in Windows Release and Linux -DNDEBUG.
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_dmc_dma_trace_0650.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    CviiiDmcDmaTrace t={0},another={0};
    const char *file="jp0650_dmc_dma_synthetic.csv";
    assert(!cviii_dmc_dma_trace_open(NULL,file));
    assert(!cviii_dmc_dma_trace_open(&t,""));
    assert(cviii_dmc_dma_trace_open(&t,file));
    assert(!cviii_dmc_dma_trace_open(&another,file));
    cviii_dmc_dma_trace_boundary("ignored",0);
    assert(t.epoch==0u && t.rows==0u);
    cviii_dmc_dma_trace_boundary("power_on",100);
    assert(t.epoch==1u && t.rows==1u);
    cviii_dmc_dma_trace_bank_write(101,0xC003u,0x91u);
    cviii_dmc_dma_trace_fetch(120,0xC040u,17u*8192u+64u,0xA5u,17u);
    cviii_dmc_dma_trace_fetch(121,0xD000u,17u*8192u+4096u,0x2Au,16u);
    cviii_dmc_dma_trace_fetch(122,0xE001u,31u*8192u+1u,0x7Fu,1u);
    cviii_dmc_dma_trace_boundary("state_loaded",10);
    assert(t.epoch==2u);
    cviii_dmc_dma_trace_fetch(11,0xC000u,3u*8192u,0xF0u,1u);
    assert(t.faults==0u && t.fetches==4u && t.bank_requests==1u);
    cviii_dmc_dma_trace_fetch(12,0xC000u,3u*8192u+1u,0x00u,1u);
    assert(t.faults==1u);
    cviii_dmc_dma_trace_bank_write(13,0xB003u,0xFFu);
    assert(t.faults==2u);
    cviii_dmc_dma_trace_fetch(9,0xC000u,3u*8192u,0xF0u,1u);
    assert(t.faults==3u);
    assert(t.rows==10u && t.fetches==6u && t.bank_requests==2u);
    cviii_dmc_dma_trace_close(&t);
    cviii_dmc_dma_trace_fetch(15,0xC000u,3u*8192u,0xF0u,1u);
    assert(t.rows==10u && !t.output);
    FILE *f=fopen(file,"r");
    assert(f);
    char line[260];unsigned n=0;
    assert(fgets(line,sizeof line,f));
    assert(!strcmp(line,
        "event,epoch,cycle,cpu_addr,physical,byte,bank8,bytes_before,requested_bank8\n"));
    while(fgets(line,sizeof line,f)) {
        ++n;
        if(n==1)assert(!strcmp(line,"power_on,1,100,-1,-1,-1,-1,-1,-1\n"));
        if(n==2)assert(!strcmp(line,"bank_request,1,101,49155,-1,-1,-1,-1,17\n"));
        if(n==3)assert(!strcmp(line,"dma_fetch,1,120,49216,139328,165,17,17,-1\n"));
        if(n==5)assert(!strcmp(line,"dma_fetch,1,122,57345,253953,127,-1,1,-1\n"));
        if(n==6)assert(!strcmp(line,"state_loaded,2,10,-1,-1,-1,-1,-1,-1\n"));
        if(n==8)assert(strstr(line,"invalid_fetch_map,2,12,49152,") == line);
        if(n==9)assert(strstr(line,"invalid_bank_addr,2,13,45059,") == line);
        if(n==10)assert(strstr(line,"invalid_cycle,2,9,49152,") == line);
    }
    fclose(f);
    assert(n==t.rows);
    assert(remove(file)==0);
    assert(cviii_dmc_dma_trace_open(&another,file));
    cviii_dmc_dma_trace_boundary("power_on",200u);
    another.rows=200000u; /* simulate a full private recording */
    cviii_dmc_dma_trace_fetch(201u,0xC000u,0u,0u,1u);
    cviii_dmc_dma_trace_fetch(202u,0xC001u,1u,0u,1u);
    assert(another.dropped==2u && another.rows==200000u);
    cviii_dmc_dma_trace_close(&another);
    f=fopen(file,"r");
    assert(f && fgets(line,sizeof line,f) && fgets(line,sizeof line,f));
    assert(fgets(line,sizeof line,f));
    assert(!strcmp(line,"trace_limit,1,201,-1,-1,-1,-1,-1,-1\n"));
    assert(!fgets(line,sizeof line,f));
    fclose(f);
    assert(remove(file)==0);
    puts("PASS JP0650 exact DMA fetch physical bank, mapper request, epochs, failed-map and cycle checks");
    return 0;
}
