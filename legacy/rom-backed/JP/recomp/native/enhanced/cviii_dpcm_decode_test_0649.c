/* JP0649 exhaustive synthetic DMC geometry + bitstream register tests.
 * No Japanese copyrighted sample data, waveform distribution or PCM parity.
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_dpcm_decode_0649.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t table[60];
static uint8_t sample_prg[CVIII_DPCM_PRG_SIZE];
static uint8_t levels[CVIII_DPCM_MAX_BITS];
static const unsigned nt[16]={428,380,340,320,286,254,226,214,
                               190,160,142,128,106,84,72,54};
static const unsigned pal[16]={398,354,316,298,276,236,210,198,
                                176,148,132,118,98,78,66,50};
static CviiiDmcPlan plan(unsigned index,unsigned addr,unsigned len,
                         unsigned rate,unsigned level)
{
    table[index*4u]=(uint8_t)rate;
    table[index*4u+1u]=(uint8_t)level;
    table[index*4u+2u]=(uint8_t)addr;
    table[index*4u+3u]=(uint8_t)len;
    CviiiDmcPlan p;
    assert(cviii_dmc_plan_0648(table,60u,0x6Fu+index,0u,&p));
    return p;
}
int main(void)
{
    unsigned valid=0,rejected=0;
    /* Entire 15x256 hypothetical original command table and 32 physical
     * VRC6 bank choices, both region rates. No fake bank evidence implied. */
    for(unsigned command=0;command<15u;++command)
        for(unsigned old=0;old<256u;++old){
            CviiiDmcPlan p=plan(command,1u,1u,command,255u);
            assert(cviii_dmc_plan_0648(table,60u,0x6Fu+command,old,&p));
            CviiiDpcmSpan s;
            if(!p.accepted){
                ++rejected;
                memset(&s,0x5A,sizeof s);
                CviiiDpcmSpan before=s;
                assert(!cviii_dpcm_span_0649(&p,0,CVIII_DPCM_NTSC,&s));
                assert(!memcmp(&before,&s,sizeof s));
            }else{
                ++valid;
                for(unsigned bank=0;bank<32u;++bank)
                    for(unsigned region=0;region<2u;++region){
                        assert(cviii_dpcm_span_0649(&p,bank,(CviiiDpcmRegion)region,&s));
                        assert(s.physical_start==bank*8192u+64u);
                        assert(s.cpu_start==0xC040u && s.cpu_end==0xC050u);
                        assert(s.byte_count==17u && s.bit_count==136u);
                        assert(s.period_cpu_cycles==(region?pal:nt)[command]);
                        assert(s.bit_period_total_cycles==136ull*s.period_cpu_cycles);
                        assert(s.initial_level==127u);
                    }
            }
        }
    assert(valid+rejected==3840u && valid>0u && rejected>0u);
    /* $4012/$4013 max addresses deliberately cross the mapper $E000
     * window. No silent use of a second, unobserved physical PRG bank. */
    CviiiDmcPlan edge=plan(0,0x7Fu,0xFFu,0u,0u);
    CviiiDpcmSpan s;
    memset(&s,0x5C,sizeof s);
    CviiiDpcmSpan untouched=s;
    assert(!cviii_dpcm_span_0649(&edge,0,CVIII_DPCM_NTSC,&s));
    assert(!memcmp(&s,&untouched,sizeof s));
    edge=plan(0,0xFFu,0u,0u,0u); /* $FFC0 outside switchable window */
    assert(!cviii_dpcm_span_0649(&edge,0,CVIII_DPCM_NTSC,&s));
    edge=plan(0,0u,0u,0u,0u);
    assert(cviii_dpcm_span_0649(&edge,31,CVIII_DPCM_NTSC,&s));
    assert(s.physical_start==31u*8192u);
    assert(s.byte_count==1u && s.bit_count==8u);
    assert(!cviii_dpcm_span_0649(&edge,32,CVIII_DPCM_NTSC,&s));
    assert(!cviii_dpcm_span_0649(&edge,0,(CviiiDpcmRegion)2,&s));
    assert(!cviii_dpcm_span_0649(NULL,0,CVIII_DPCM_NTSC,&s));
    assert(!cviii_dpcm_span_0649(&edge,0,CVIII_DPCM_NTSC,NULL));
    /* Least-significant-bit first. DAC 64 and byte 0x03:
     * 66,68,66,64,62,60,58,56. */
    edge=plan(0,0u,0u,15u,64u);
    assert(cviii_dpcm_span_0649(&edge,9,CVIII_DPCM_NTSC,&s));
    sample_prg[s.physical_start]=3u;
    uint8_t final=0;
    assert(cviii_dpcm_bits_0649(sample_prg,sizeof sample_prg,&s,levels,8u,&final));
    const uint8_t expected[]={66,68,66,64,62,60,58,56};
    assert(!memcmp(levels,expected,8u) && final==56u);
    sample_prg[s.physical_start]=0xffu;
    edge=plan(0,0u,0u,0u,126u);
    assert(cviii_dpcm_span_0649(&edge,9,CVIII_DPCM_NTSC,&s));
    assert(cviii_dpcm_bits_0649(sample_prg,sizeof sample_prg,&s,levels,8,&final));
    assert(final==126u); /* positive clamp no wrap */
    sample_prg[s.physical_start]=0u;
    edge=plan(0,0u,0u,0u,1u);
    assert(cviii_dpcm_span_0649(&edge,9,CVIII_DPCM_NTSC,&s));
    assert(cviii_dpcm_bits_0649(sample_prg,sizeof sample_prg,&s,levels,8,&final));
    assert(final==1u); /* negative clamp keeps low DAC bit */
    edge=plan(0,0x7Fu,0u,2u,12u);
    assert(cviii_dpcm_span_0649(&edge,31,CVIII_DPCM_NTSC,&s));
    assert(s.physical_start==31u*8192u+8128u);
    sample_prg[s.physical_start]=0xAAu;
    assert(cviii_dpcm_bits_0649(sample_prg,sizeof sample_prg,&s,levels,8,&final));
    memset(levels,0xA5,sizeof levels); final=73u;
    assert(!cviii_dpcm_bits_0649(sample_prg,sizeof sample_prg,&s,levels,7,&final));
    assert(final==73u && levels[0]==0xA5u);
    CviiiDpcmSpan damaged=s;damaged.physical_start++;
    assert(!cviii_dpcm_bits_0649(sample_prg,sizeof sample_prg,&damaged,levels,8,&final));
    assert(final==73u && levels[0]==0xA5u);
    assert(!cviii_dpcm_bits_0649(sample_prg,1u,&s,levels,8,&final));
    assert(final==73u && levels[0]==0xA5u);
    CviiiDmcPlan corrupted=edge;
    corrupted.writes[0].value=0x1Fu;
    assert(!cviii_dpcm_span_0649(&corrupted,0,CVIII_DPCM_NTSC,&s));
    puts("PASS JP0649 ROM-free DPCM: 3840 guards, 32 banks, NTSC/PAL rates, bit order, clamping, bounds");
    return 0;
}
