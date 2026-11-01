/* JP0655: ROM-free exhaustive synthetic byte arithmetic regression.
 * The oracle uses a slow 6502-style repeat-add loop, independently of
 * the native multiplication implementation. Retail sound data excluded. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_phrase_delta_0655.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static uint8_t bank[16384], ram[2048];
static uint8_t repeated(uint8_t amount, uint8_t multiplier) {
    uint8_t a = amount;
    uint8_t x = multiplier;
    do {
        --x;
        if (!x) break;
        a = (uint8_t)(a + amount);
    } while (1);
    return a;
}
static void test_case(unsigned token, unsigned factor, unsigned lo,
                      unsigned hi, unsigned entry, unsigned restored) {
    CviiiPhraseDelta0655 o;
    uint8_t mag, lower, upper, c, mutated, n, z, v;
    unsigned source = 0xA500u;
    memset(ram, 0, sizeof ram);
    ram[0xE2] = 0xF0;
    ram[0xE3] = 0xA4;
    ram[0x06AA+entry] = 0x10;
    ram[0x03F7+entry] = (uint8_t)factor;
    ram[0x03F2+entry] = 0xB9;
    ram[0xEE] = (uint8_t)restored;
    ram[0x01AD+restored] = (uint8_t)lo;
    ram[0x01B2+restored] = (uint8_t)hi;
    bank[source-0x8000u] = (uint8_t)token;
    assert(cviii_phrase_delta_0655(bank, sizeof bank, ram, sizeof ram,
                                   (uint8_t)entry, &o) == 1);
    mag = repeated((uint8_t)((token&15u) < 8u ? token&15u : 16u-(token&15u)),
                   (uint8_t)factor);
    if ((token&15u) < 8u) {
        unsigned sum=lo+mag;
        lower=(uint8_t)sum;
        c=(uint8_t)(sum > 255u);
        upper=(uint8_t)(hi+c);
        mutated=c;
        v=(uint8_t)((~(mag^lo)&(mag^lower)&0x80u)!=0u);
    } else {
        lower=(uint8_t)(lo-mag);
        c=(uint8_t)(lo>=mag);
        mutated=(uint8_t)!c;
        upper=(uint8_t)(hi-mutated);
        v=(uint8_t)(((lo^mag)&(lo^lower)&0x80u)!=0u);
    }
    n=(uint8_t)(((mutated ? upper : hi)>>7u)&1u);
    z=(uint8_t)((mutated ? upper : hi)==0);
    assert(o.source_byte==token && o.source_cpu_address==source);
    assert(o.entry_x==entry && o.exit_x==restored);
    assert(o.exit_y==0x10 && o.exit_a==hi && o.exit_c==c);
    assert(o.exit_n==n && o.exit_z==z && o.exit_v==v);
    assert(o.cursor_after==0x11);
    assert(o.did_update_high_nibble==(token>=16u));
    assert(o.high_nibble_after==(token>=16u ? token>>4u : 0xB9u));
    assert(o.scratch_e2_after==mag && o.low_ec_after==lower && o.high_ed_after==upper);
}
static void reject_cases(void) {
    CviiiPhraseDelta0655 out, prior;
    memset(&out,0x5A,sizeof out);
    prior=out;
    memset(bank,0,sizeof bank);
    memset(ram,0,sizeof ram);
    ram[0xE2]=0x00;
    ram[0xE3]=0xA5;
    for (unsigned cmd=0xFB;cmd<=0xFF;cmd++) {
        bank[0xA500-0x8000]= (uint8_t)cmd;
        assert(cviii_phrase_delta_0655(bank,sizeof bank,ram,sizeof ram,0,&out)==0);
        assert(!memcmp(&prior,&out,sizeof out));
    }
    assert(cviii_phrase_delta_0655(bank,sizeof bank-1,ram,sizeof ram,0,&out)==-1);
    assert(!memcmp(&prior,&out,sizeof out));
    assert(cviii_phrase_delta_0655(bank,sizeof bank,ram,sizeof ram-1,0,&out)==-1);
    assert(!memcmp(&prior,&out,sizeof out));
    assert(cviii_phrase_delta_0655(NULL,sizeof bank,ram,sizeof ram,0,&out)==-1);
    assert(!memcmp(&prior,&out,sizeof out));
    assert(cviii_phrase_delta_0655(bank,sizeof bank,NULL,sizeof ram,0,&out)==-1);
    assert(!memcmp(&prior,&out,sizeof out));
    assert(cviii_phrase_delta_0655(bank,sizeof bank,ram,sizeof ram,0,NULL)==-1);
    ram[0xE3]=0xC0;
    assert(cviii_phrase_delta_0655(bank,sizeof bank,ram,sizeof ram,0,&out)==-1);
    assert(!memcmp(&prior,&out,sizeof out));
    ram[0xE3]=0x7F;
    assert(cviii_phrase_delta_0655(bank,sizeof bank,ram,sizeof ram,0,&out)==-1);
    assert(!memcmp(&prior,&out,sizeof out));
}
int main(void) {
    const unsigned base[][2]={{0,0},{255,255},{0,255},{255,0},{127,128},{128,127}};
    unsigned count=0;
    for (unsigned token=0;token<251u;token++) {
        for (unsigned factor=0;factor<256u;factor++) {
            for (unsigned k=0;k<6u;k++) {
                test_case(token,factor,base[k][0],base[k][1],
                          1u+k,7u+k);
                count++;
            }
        }
    }
    assert(count==385536u);
    reject_cases();
    return 0;
}
