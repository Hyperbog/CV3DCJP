/* JP0649: source-validated static 2A03 DMC sample geometry + raw 7-bit DAC.
 * No guest CPU/PPU/APU/mapper writes, audio_mix integration or bitstream
 * extraction from retail ROM. Physical bank MUST be supplied by observer.
 */
#include "cviii_dpcm_decode_0649.h"
#include <string.h>
static const uint16_t rate_ntsc[16] = {
    428,380,340,320,286,254,226,214,190,160,142,128,106,84,72,54
};
static const uint16_t rate_pal[16] = {
    398,354,316,298,276,236,210,198,176,148,132,118,98,78,66,50
};
bool cviii_dpcm_span_0649(const CviiiDmcPlan *p,unsigned bank8,
                          CviiiDpcmRegion region,CviiiDpcmSpan *out)
{
    if(!p || !out || !p->accepted || p->write_count!=CVIII_DMC_WRITES ||
       p->command<0x6Fu || p->command>0x7Du ||
       p->index!=(unsigned)p->command-0x6Fu || bank8>=32u ||
       (region!=CVIII_DPCM_NTSC && region!=CVIII_DPCM_PAL) ||
       p->writes[0].address!=0x4015u || p->writes[0].value!=0x0Fu ||
       p->writes[5].address!=0x4015u || p->writes[5].value!=0x1Fu ||
       p->priority_after!=p->index ||
       p->counter_increment!=(p->index>=13u))return false;
    const uint16_t addresses[4]={0x4010u,0x4011u,0x4012u,0x4013u};
    for(unsigned i=0;i<4u;++i)if(p->writes[i+1u].address!=addresses[i])
        return false;
    const unsigned start=0xC000u+(unsigned)p->writes[3].value*64u;
    const unsigned n=(unsigned)p->writes[4].value*16u+1u;
    if(start<0xC000u || start>0xDFFFu ||
       n>CVIII_DPCM_MAX_SAMPLE_BYTES || start+n>0xE000u)return false;
    CviiiDpcmSpan s;
    memset(&s,0,sizeof s);
    s.command=p->command;s.index=p->index;s.bank8=(uint8_t)bank8;
    s.rate_index=p->writes[1].value&15u;
    s.irq_enabled=(p->writes[1].value&0x80u)!=0u;
    s.loop=(p->writes[1].value&0x40u)!=0u;
    s.initial_level=p->writes[2].value&127u;
    s.cpu_start=(uint16_t)start;
    s.cpu_end=(uint16_t)(start+n-1u);
    s.byte_count=(uint32_t)n;
    s.bit_count=(uint32_t)(n*8u);
    s.physical_start=(uint32_t)(bank8*CVIII_DPCM_BANK_SIZE+start-0xC000u);
    s.period_cpu_cycles=(region==CVIII_DPCM_NTSC?rate_ntsc:rate_pal)[s.rate_index];
    s.bit_period_total_cycles=(uint64_t)s.bit_count*s.period_cpu_cycles;
    *out=s;
    return true;
}
bool cviii_dpcm_bits_0649(const uint8_t *prg,size_t prg_size,
                          const CviiiDpcmSpan *s,uint8_t *output,
                          size_t capacity,uint8_t *final_level)
{
    if(!prg || !s || !output || !final_level ||
       prg_size!=CVIII_DPCM_PRG_SIZE || s->bank8>=32u ||
       s->byte_count==0u || s->byte_count>CVIII_DPCM_MAX_SAMPLE_BYTES ||
       s->bit_count!=s->byte_count*8u || capacity<s->bit_count ||
       s->initial_level>127u || s->cpu_start<0xC000u ||
       s->cpu_start>0xDFFFu ||
       (unsigned)s->cpu_start+s->byte_count>0xE000u ||
       s->cpu_end!=(uint16_t)((unsigned)s->cpu_start+s->byte_count-1u) ||
       s->physical_start!=(uint32_t)(s->bank8*CVIII_DPCM_BANK_SIZE+
                                             s->cpu_start-0xC000u) ||
       (uint64_t)s->physical_start+s->byte_count>prg_size)
        return false;
    unsigned level=s->initial_level;
    size_t k=0;
    for(unsigned i=0;i<s->byte_count;++i) {
        const uint8_t bits=prg[s->physical_start+i];
        for(unsigned j=0;j<8u;++j){
            if((bits>>j)&1u) {
                if(level<=125u)level+=2u;
            }else if(level>=2u)level-=2u;
            output[k++]=(uint8_t)level;
        }
    }
    *final_level=(uint8_t)level;
    return true;
}
