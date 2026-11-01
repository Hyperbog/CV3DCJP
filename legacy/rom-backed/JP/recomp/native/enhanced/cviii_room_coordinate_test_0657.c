#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_room_coordinate_0657.h"
#include <assert.h>
#include <string.h>
static uint8_t bank[16384],ram[2048];
int main(void) {
  CviiiRoomCoordinate0657 v,keep;
  unsigned cases=0;
  /* Independent 16-bit two-bit rotate oracle, never formula-matches
   * the two 6502 ROL $73 operations with host's shift expression. */
  for(unsigned lo=0;lo<256;lo++)for(unsigned hi=0;hi<256;hi++)for(unsigned bit=0;bit<2;bit++){
    unsigned c, val, high, cl, upper, sign,sub,base,mask,offset,newhigh;
    memset(ram,0,sizeof ram);
    ram[0x53]=(uint8_t)lo;ram[0x54]=(uint8_t)hi;
    ram[0x62]=(uint8_t)(bit?255:0);
    cl=(lo+2)&255; c=(lo+2)>255; high=(hi+c)&255; val=cl;
    for(unsigned turn=0;turn<2;turn++){
      c=val>>7; val=(val<<1)&255;
      unsigned carryout=high>>7;
      high=((high<<1)&255)|c; c=carryout;
    }
    upper=high;
    sub=bit?5:0;base=bit?0:192;
    mask=lo&63;sign=mask&32?(mask|192):mask;
    offset=(sign+base)&255;newhigh=(upper+sub)&255;
    assert(cviii_room_coordinate_0657(bank,sizeof bank,ram,sizeof ram,&v)==1);
    assert(v.a==1&&v.x==ram[0x62]&&v.y==newhigh);
    assert(v.room_hi_73==newhigh&&v.room_offset_09==offset&&v.room_sub_08==sub);
    assert(v.masked_0e==mask&&v.room_phase_0a==1);
    assert(v.carry==((upper+sub)>255));
    assert(v.overflow==((~(upper^sub)&(upper^newhigh)&128)!=0));
    assert(v.zero==(newhigh==0)&&v.negative==((newhigh>>7)&1));
    cases++;
  }
  assert(cases==131072);
  memset(&v,0xA5,sizeof v);keep=v;
  assert(cviii_room_coordinate_0657(NULL,sizeof bank,ram,sizeof ram,&v)==-1);
  assert(!memcmp(&v,&keep,sizeof v));
  assert(cviii_room_coordinate_0657(bank,sizeof bank-1,ram,sizeof ram,&v)==-1);
  assert(!memcmp(&v,&keep,sizeof v));
  assert(cviii_room_coordinate_0657(bank,sizeof bank,ram,sizeof ram-1,&v)==-1);
  assert(!memcmp(&v,&keep,sizeof v));
  assert(cviii_room_coordinate_0657(bank,sizeof bank,NULL,sizeof ram,&v)==-1);
  assert(!memcmp(&v,&keep,sizeof v));
  assert(cviii_room_coordinate_0657(bank,sizeof bank,ram,sizeof ram,NULL)==-1);
  return 0;
}
