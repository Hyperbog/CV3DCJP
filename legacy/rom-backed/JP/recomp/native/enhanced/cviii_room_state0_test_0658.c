#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_room_state0_0658.h"
#include <assert.h>
#include <string.h>
static uint8_t fixed[8192],raw[16384],ram[2048];
static unsigned get(unsigned p){return fixed[p-0xE000u];}
static void put(unsigned p,unsigned v){fixed[p-0xE000u]=(uint8_t)v;}
/* Entirely invented test stream: two synthetic nametables, synthetic
 * nonzero values, distinct from the 42-byte retail PPU clear script. */
static void fixture(void) {
    unsigned pos=0xEC1Du;
    memset(fixed,0,sizeof fixed);
    put(0xEBF7u,0x1Du);put(0xEBF8u,0xECu);
    put(pos++,0);put(pos++,0x21u);
    for(unsigned block=0;block<2;block++) {
        if(block) {put(pos++,0x7Fu);put(pos++,0);put(pos++,0x25u);}
        for(unsigned k=0;k<9u;k++){
            put(pos++,k<7u?0x70u:0x78u);
            put(pos++,(unsigned)(1u+block*0x30u+k));
        }
    }
    put(pos++,0xFFu);
    assert(pos==0xEC47u);
}
static int was_zeroed(unsigned addr) {
    return (addr>=0x4du&&addr<0xe0u) ||
     (addr>=0xf0u&&addr<0xf8u) ||
     (addr>=0x300u&&addr<0x3c0u) ||
     (addr>=0x400u&&addr<0x500u) ||
     (addr>=0x500u&&addr<0x600u) ||
     (addr>=0x600u&&addr<0x6a0u) ||
     (addr>=0x6e0u&&addr<0x700u) ||
     (addr>=0x700u&&addr<0x7f6u);
}
int main(void) {
    CviiiRoomState0_0658 s,prior;
    CviiiRoomPpuStream0658 p,oldp;
    unsigned cases=0u;
    fixture();
    for(unsigned seed=0;seed<256u;seed++){
      for(unsigned inc=0;inc<2u;inc++){
        memset(ram,0,sizeof ram);
        for(unsigned i=0;i<sizeof ram;i++)ram[i]=(uint8_t)(seed*13u+i*7u);
        ram[0x18u]=0x0du;ram[0x19u]=0;ram[0x23u]=0x0au;
        ram[0xffu]=(uint8_t)(inc?0x84u:0x80u);
        ram[0xfeu]=(uint8_t)(seed*3u);
        ram[0x781u]=(uint8_t)(seed^0x55u);
        assert(cviii_room_state0_0658(raw,16384,fixed,8192,ram,2048,&s)==1);
        assert(s.ppu.count==2048 && s.ppu.consumed_stream_bytes==42);
        assert(s.ppu.final_source_pointer==0xEC46u && s.ppu.address_switch_count==2u);
        assert(s.ppu.original_ppu_status_reads==2);
        assert(s.ppu.ctrl_final==ram[0xffu] && s.ppu.mask_setup==(ram[0xfeu]&0xe7u));
        assert(s.ppu.address[0]==0x2100u && s.ppu.address[1024]==0x2500u);
        assert(s.ppu.address[1]==((0x2100u+(inc?32u:1u))&0x3fffu));
        assert(s.ppu.address[1023]==((0x2100u+1023u*(inc?32u:1u))&0x3fffu));
        assert(s.ppu.address[2047]==((0x2500u+1023u*(inc?32u:1u))&0x3fffu));
        assert(s.ppu.final_vram_address==((0x2500u+1024u*(inc?32u:1u))&0x3fffu));
        assert(s.ppu.value[0]==1 && s.ppu.value[111]==1);
        assert(s.ppu.value[112]==2 && s.ppu.value[1024]==0x31u);
        assert(s.ppu.scratch02_final_count==0x78u);
        assert(s.distinct_cleared_bytes==1297u && s.cpu_ram_bank_after==0x0au);
        assert(s.mapper_reg_address==0xc000u && s.mapper_reg_value==0x1eu);
        assert(s.sound_reg_address==0xb003u && s.sound_reg_value==0x20u);
        assert(s.state_after==1 && s.ram_after[0x19]==1);
        assert(s.ram_after[0]==0x46u && s.ram_after[1]==0xecu && s.ram_after[2]==0x78u);
        assert(s.ram_after[0x781u]==ram[0x781u]);
        for(unsigned addr=0;addr<2048u;addr++){
          if(addr>=0x100u && addr<0x200u)continue; /* CPU stack is excluded */
          unsigned expected=ram[addr];
          if(was_zeroed(addr))expected=0;
          if(addr==0x781u)expected=ram[addr];
          if(addr==0x21u)expected=0x20u;
          if(addr==0x6au||addr==0x43u||addr==0x40u||addr==0x6fu)expected=0;
          if(addr==0x00u)expected=0x46u;
          if(addr==0x01u)expected=0xecu;
          if(addr==0x02u)expected=0x78u;
          if(addr==0xfcu||addr==0xfdu)expected=0;
          if(addr==0x19u)expected=1;
          assert(s.ram_after[addr]==expected);
        }
        cases++;
      }
    }
    assert(cases==512u);
    /* Invalid bank, incomplete source stream, malformed PPU commands and
     * unsatisfied source-state assumptions must not mutate prior outputs. */
    memset(&s,0x5A,sizeof s);prior=s;
    memset(&p,0xA5,sizeof p);oldp=p;
    ram[0x19]=0;
    assert(cviii_room_state0_0658(raw,16383,fixed,8192,ram,2048,&s)==-1);
    assert(!memcmp(&s,&prior,sizeof s));
    assert(cviii_room_state0_0658(raw,16384,fixed,8191,ram,2048,&s)==-1);
    assert(!memcmp(&s,&prior,sizeof s));
    assert(cviii_room_state0_0658(raw,16384,fixed,8192,ram,2047,&s)==-1);
    assert(!memcmp(&s,&prior,sizeof s));
    assert(cviii_room_state0_0658(NULL,16384,fixed,8192,ram,2048,&s)==-1);
    assert(!memcmp(&s,&prior,sizeof s));
    assert(cviii_room_state0_0658(raw,16384,fixed,8192,NULL,2048,&s)==-1);
    assert(!memcmp(&s,&prior,sizeof s));
    assert(cviii_room_state0_0658(raw,16384,fixed,8192,ram,2048,NULL)==-1);
    ram[0x18]=0;
    assert(cviii_room_state0_0658(raw,16384,fixed,8192,ram,2048,&s)==-1);
    assert(!memcmp(&s,&prior,sizeof s));
    ram[0x18]=0x0d;
    assert(cviii_room_ppu_stream_0658(fixed,8191,ram[0xff],ram[0xfe],&p)==-1);
    assert(!memcmp(&p,&oldp,sizeof p));
    unsigned old=get(0xEBF7u);put(0xEBF7u,0);
    assert(cviii_room_ppu_stream_0658(fixed,8192,0,0,&p)==-1);
    assert(!memcmp(&p,&oldp,sizeof p));put(0xEBF7u,old);
    old=get(0xEC46u);put(0xEC46u,0);
    assert(cviii_room_ppu_stream_0658(fixed,8192,0,0,&p)==-1);
    assert(!memcmp(&p,&oldp,sizeof p));put(0xEC46u,old);
    old=get(0xEC1Fu);put(0xEC1Fu,0);
    assert(cviii_room_ppu_stream_0658(fixed,8192,0,0,&p)==-1);
    assert(!memcmp(&p,&oldp,sizeof p));put(0xEC1Fu,old);
    assert(cviii_room_ppu_stream_0658(fixed,8192,0,0,&p)==1);
    return 0;
}
