/* ROM-free original-room stream parity contract; owner-local pack optional. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_room_native_bridge_0660.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static CviiiRoomNativeBridge0660 b;
static uint8_t ram[2048],saved[2048];
static void synthetic(void) {
    static const unsigned roots[]={0xBEA4u,0xBEDBu,0xBF03u,0xBE8Bu,0xBF1Cu};
    static const unsigned counts[]={10,7,4,4,5};
    b.ready=1;
    for(unsigned k=0;k<5;k++) {
        unsigned t=0xB8C2u-0x8000u+2u*k;
        b.bank0a[t]=(uint8_t)roots[k]; b.bank0a[t+1]=(uint8_t)(roots[k]>>8u);
        unsigned root=roots[k]-0x8000u;
        for(unsigned i=0;i<4;i++) b.bank0e[root+i]=(uint8_t)(k+10u+i);
        for(unsigned i=0;i<counts[k];i++)for(unsigned j=0;j<5;j++)
            b.bank0e[root+4u+5u*i+j]=(uint8_t)(1u+k*11u+i*17u+j*23u);
        b.bank0e[root+4u+5u*counts[k]]=0xFFu;
    }
}
static void seed(unsigned sel,unsigned n) {
    for(unsigned i=0;i<sizeof ram;i++) ram[i]=(uint8_t)(i*19u+n*37u);
    ram[0x23]=0x0Au;ram[0x7A0]=(uint8_t)sel;
    ram[0x1FD]=0x34u;ram[0x1FE]=0x12u; /* S=FC -> return PC 1235 */
}
int main(int argc,char **argv) {
    synthetic();
    unsigned tested=0;
    for(unsigned sel=0;sel<5;sel++) for(unsigned n=0;n<256;n++) {
        seed(sel,n);
        memcpy(saved,ram,sizeof ram);
        int traced=(n&1u)!=0;
        assert(cviii_bridge0660_begin(&b,ram,sizeof ram,0xFCu,
               (uint8_t)(n&1u),1,0,100u+tested,traced)==1);
        assert(!memcmp(saved,ram,sizeof ram)); /* verifier never writes CPU RAM */
        assert(cviii_bridge0660_rts(&b,ram,sizeof ram,0x9999u,
               0,0,0,0,0,0,0,0,1,0,0x2Bu)==0); /* nested RTS ignored */
        if(traced) for(unsigned i=0;i<b.predicted.mapper_events;i++)
            cviii_bridge0660_mapper_write(&b,b.predicted.mapper_values[i]);
        memcpy(ram,b.predicted.ram_after,sizeof ram);
        /* Nested calls overwrite genuine stack scratch; excluded from RAM diff. */
        ram[0x1A0] ^= 0xA5u;
        int result=cviii_bridge0660_rts(&b,ram,sizeof ram,0x1235u,
            b.predicted.a_after,b.predicted.x_after,b.predicted.source_y_after,
            0xFEu,b.predicted.carry_after,b.predicted.zero_after,
            b.predicted.negative_after,b.predicted.overflow_after,1,0,0x2Bu);
        assert(result==(traced?1:2));
        assert(b.active==0);
        tested++;
    }
    assert(tested==1280u && b.observed_entries==tested &&
           b.passed==640u && b.partial==640u && b.mismatched==0);
    seed(0,0);
    assert(cviii_bridge0660_begin(&b,ram,sizeof ram,0xFC,0,1,0,100,1)==1);
    memcpy(ram,b.predicted.ram_after,sizeof ram);
    for(unsigned i=0;i<b.predicted.mapper_events;i++)
        cviii_bridge0660_mapper_write(&b,b.predicted.mapper_values[i]);
    ram[0x63u]^=1u; /* divergence leaves original game RAM unchanged */
    assert(cviii_bridge0660_rts(&b,ram,sizeof ram,0x1235u,
        b.predicted.a_after,b.predicted.x_after,b.predicted.source_y_after,
        0xFE,1,1,0,0,1,0,0x2Bu)==-1);
    assert(b.mismatched==1u);
    seed(4,2);
    assert(cviii_bridge0660_begin(&b,ram,sizeof ram,0xFC,0,1,0,100,1)==1);
    memcpy(ram,b.predicted.ram_after,sizeof ram);
    for(unsigned i=0;i<b.predicted.mapper_events;i++)
        cviii_bridge0660_mapper_write(&b,(uint8_t)(b.predicted.mapper_values[i]^1u));
    assert(cviii_bridge0660_rts(&b,ram,sizeof ram,0x1235u,
        b.predicted.a_after,b.predicted.x_after,b.predicted.source_y_after,
        0xFE,1,1,0,0,1,0,0x2Bu)==-1);
    assert(b.mismatched==2u);
    seed(5,0);
    assert(!cviii_bridge0660_begin(&b,ram,sizeof ram,0xFC,0,1,0,100,1));
    assert(!b.active && b.skipped==1);
    seed(0,0);
    assert(cviii_bridge0660_begin(&b,ram,sizeof ram,0xFC,0,1,0,100,0)==1);
    cviii_bridge0660_reset(&b);
    assert(b.partial==641u && b.active==0);
    if(argc>1) {
        assert(cviii_bridge0660_load(&b,argv[1]));
        for(unsigned sel=0;sel<5;sel++) {
            seed(sel,0);
            assert(cviii_bridge0660_begin(&b,ram,sizeof ram,0xFC,0,1,0,100,1)==1);
            assert(b.predicted.source_calls>=27);
            cviii_bridge0660_reset(&b);
        }
        printf("PASS authentic-owner pack for five original source streams\n");
    }
    puts("PASS 1280 synthetic original/native postconditions, 640 mapper traces, mismatch/fallback guards");
    return 0;
}