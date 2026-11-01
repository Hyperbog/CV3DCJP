#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_room_stream_0659.h"
#include <assert.h>
#include <string.h>
static uint8_t bank0a[16384],bank0e[16384],ram[2048],expected[2048];
static const unsigned roots[]={0xBEA4u,0xBEDBu,0xBF03u,0xBE8Bu,0xBF1Cu};
static const unsigned counts[]={10u,7u,4u,4u,5u};
static unsigned src(unsigned cpu){return bank0e[cpu-0x8000u];}
static void make_source(void) {
    memset(bank0a,0,sizeof bank0a);memset(bank0e,0,sizeof bank0e);
    for (unsigned sel=0;sel<5;sel++) {
        unsigned p=roots[sel]-0x8000u;
        bank0a[0xB8C2u-0x8000u+2u*sel]=(uint8_t)roots[sel];
        bank0a[0xB8C3u-0x8000u+2u*sel]=(uint8_t)(roots[sel]>>8u);
        for (unsigned j=0;j<4;j++)bank0e[p+j]=(uint8_t)(17u+sel*19u+j*11u);
        for (unsigned j=0;j<counts[sel];j++){
            for (unsigned k=0;k<5;k++)
                bank0e[p+4u+5u*j+k]=(uint8_t)((sel*13u+j*17u+k*29u+1u)&255u);
            if(j==0) bank0e[p+4u+5u*j+1u]=255u; /* FF ordinary field allowed */
        }
        bank0e[p+4u+counts[sel]*5u]=255u;
    }
}
static void expected_output(unsigned sel) {
    unsigned root=roots[sel];
    memcpy(expected,ram,sizeof expected);
    expected[0x08u]=(uint8_t)root;
    expected[0x09u]=(uint8_t)(root>>8);
    expected[0x34u]=(uint8_t)(src(root)>>4u);
    expected[0x35u]=(uint8_t)(src(root)&15u);
    expected[0x36u]=(uint8_t)(src(root+1u)>>4u);
    expected[0x54u]=(uint8_t)(src(root+1u)&15u);
    expected[0x72u]=(uint8_t)(src(root+1u)&1u);
    expected[0x53u]=0;
    expected[0x7A1u]=(uint8_t)src(root+2u);
    expected[0x54Eu]=(uint8_t)src(root+3u);
    for(unsigned j=0;j<counts[sel];j++){
        unsigned start=root+4u+j*5u;
        expected[0x48Cu+j+1u]=(uint8_t)src(start);
        expected[0x400u+j+1u]=(uint8_t)src(start+1u);
        expected[0x438u+j+1u]=(uint8_t)src(start+2u);
        expected[0x41Cu+j+1u]=(uint8_t)src(start+3u);
        expected[0x4A8u+j+1u]=(uint8_t)src(start+4u);
        expected[0x54Eu+j+1u]=1u;
    }
}
int main(void) {
    unsigned tests=0;
    make_source();
    for(unsigned sel=0;sel<5;sel++)for(unsigned seed=0;seed<256;seed++){
        CviiiRoomStream0659 out;
        memset(ram,0,sizeof ram);
        for(unsigned i=0;i<2048;i++)ram[i]=(uint8_t)((i*17u+seed*31u)&255u);
        ram[0x7A0u]=(uint8_t)sel;ram[0x23u]=10u;
        expected_output(sel);
        assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,
                   ram,2048,(uint8_t)(seed&1u),&out)==1);
        assert(memcmp(out.ram_after,expected,2048)==0);
        assert(out.source_root==roots[sel]&&out.record_count==counts[sel]);
        assert(out.x_after==counts[sel]+1u&&out.source_y_after==4u+5u*counts[sel]);
        assert(out.a_after==255u&&out.carry_after==1&&out.zero_after==1&&out.negative_after==0);
        assert(out.overflow_after==(seed&1u)&&out.bank_after==10);
        assert(out.source_calls==7u+5u*counts[sel]);
        assert(out.mapper_events==2u*out.source_calls);
        for(unsigned j=0;j<out.mapper_events;j++)
            assert(out.mapper_values[j]==(j&1u?10u:14u));
        tests++;
    }
    assert(tests==1280u);
    CviiiRoomStream0659 out,unchanged;
    memset(&out,0xA5,sizeof out);unchanged=out;
    ram[0x23u]=10u;
    ram[0x7A0u]=5u;
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,ram,2048,0,&out)==0);
    assert(!memcmp(&out,&unchanged,sizeof out));
    ram[0x7A0u]=0u;
    assert(cviii_room_stream_0659(bank0a,16383,bank0e,16384,ram,2048,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16383,ram,2048,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,ram,2047,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));
    assert(cviii_room_stream_0659(NULL,16384,bank0e,16384,ram,2048,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));
    assert(cviii_room_stream_0659(bank0a,16384,NULL,16384,ram,2048,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,NULL,2048,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,ram,2048,2,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));
    ram[0x23u]=14;
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,ram,2048,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));
    ram[0x23u]=10;
    unsigned off=0xB8C2u-0x8000u,prior=bank0a[off];
    bank0a[off]=0;
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,ram,2048,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));bank0a[off]=(uint8_t)prior;
    off=roots[0]-0x8000u+4u+5u*counts[0];prior=bank0e[off];bank0e[off]=0;
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,ram,2048,0,&out)==-1);
    assert(!memcmp(&out,&unchanged,sizeof out));bank0e[off]=(uint8_t)prior;
    assert(cviii_room_stream_0659(bank0a,16384,bank0e,16384,ram,2048,0,NULL)==-1);
    return 0;
}
