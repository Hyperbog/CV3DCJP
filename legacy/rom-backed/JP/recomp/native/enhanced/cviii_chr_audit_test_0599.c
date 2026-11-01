/* JP0599 ROM-free pinned mapper-dump parse and mapping tests. */
#include "cviii_chr_audit_0599.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FILE *make(unsigned ctrl, const unsigned regs[8],
                  const unsigned offset[8])
{
    FILE *f = tmpfile();
    assert(f);
    fprintf(f,"cart.ctrl %02X\n",ctrl);
    for (unsigned i=0;i<8;++i) fprintf(f,"cart.vrc_chr[%u] %03X\n",i,regs[i]);
    for (unsigned i=0;i<8;++i) fprintf(f,"cart.chr_off[%u] %06X\n",i,offset[i]);
    rewind(f);
    return f;
}
static void probe(unsigned ctrl, const unsigned regs[8],
                  const unsigned expected[8])
{
    unsigned offsets[8];
    for (unsigned i=0;i<8;++i) offsets[i]=expected[i]*1024u;
    FILE *snapshot=make(ctrl,regs,offsets);
    CviiiChrBanks banks={0};
    assert(cviii_chr_banks_read(snapshot,&banks));
    assert(cviii_chr_banks_check(&banks));
    for (unsigned i=0;i<8;++i) assert(banks.banks[i] == expected[i]);
    fclose(snapshot);
}
int main(void)
{
    const unsigned regs[8]={2,4,6,8,10,12,14,0x83};
    const unsigned mode0[8]={2,4,6,8,10,12,14,3};
    const unsigned mode1[8]={2,2,4,4,6,6,8,8};
    const unsigned mode2[8]={2,4,6,8,10,10,12,12};
    const unsigned mode3[8]={2,4,6,8,10,10,12,12};
    const unsigned mode1_pairs[8]={2,3,4,5,6,7,8,9};
    const unsigned mode2_pairs[8]={2,4,6,8,10,11,12,13};
    probe(0,regs,mode0);
    probe(1,regs,mode1);
    probe(2,regs,mode2);
    probe(3,regs,mode3);
    probe(0x21,regs,mode1_pairs);
    probe(0x22,regs,mode2_pairs);
    probe(0x23,regs,mode2_pairs);
    /* Snapshot offsets are physical BYTE offsets, not bank IDs. */
    unsigned wrong[8]={2*1024,4*1024,6*1024,8*1024,
                       10*1024,11*1024,12*1024,13*1024};
    FILE *f=make(0x22,regs,wrong);
    CviiiChrBanks b;
    assert(cviii_chr_banks_read(f,&b));
    assert(cviii_chr_banks_check(&b)); /* this one is actually correct */
    fclose(f);
    wrong[4] = 12*1024;
    f=make(0x22,regs,wrong);
    assert(cviii_chr_banks_read(f,&b));
    assert(!cviii_chr_banks_check(&b));
    fclose(f);
    f=make(0x22,regs,mode0); /* function needs byte offsets, not banks */
    assert(!cviii_chr_banks_read(f,&b));
    fclose(f);
    puts("PASS JP0599 VRC6 CHR modes 0-3 and bit5 pair override");
    return 0;
}
