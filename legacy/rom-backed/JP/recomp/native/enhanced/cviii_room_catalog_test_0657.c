#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_room_catalog_0657.h"
#include <assert.h>
#include <string.h>
static uint8_t b[16384];
static unsigned rd(unsigned cpu){return b[cpu-0x8000u];}
static void wr(unsigned cpu,unsigned v){b[cpu-0x8000u]=(uint8_t)v;}
static void word(unsigned addr,unsigned v){wr(addr,v);wr(addr+1u,v>>8u);}
static void artificial(void){
  const unsigned groups[15]={4,6,5,5,3,4,3,7,5,2,7,3,3,4,3};
  unsigned second[64],leaf[135],second_count=0,leaf_count=0;
  unsigned st=0x92CCu;
  memset(b,0,sizeof b);
  for(unsigned i=0;i<15u;i++){
    if(i==9u)st=0x9AF4u;
    word(0x92AEu+2u*i,st);st+=2u*groups[i];
  }
  assert(st==0x9B20u);
  st=0x9320u;
  for(unsigned i=0;i<64u;i++){
    if(i==42u)st=0x9B20u;
    second[i]=st;
    unsigned n=2u+(i<2u || (i>=42u && i<47u));
    st+=2u*n;
  }
  assert(st==0x9B82u);
  unsigned cursor=0x92CCu;
  for(unsigned i=0;i<15u;i++){
    if(i==9u)cursor=0x9AF4u;
    for(unsigned j=0;j<groups[i];j++)word(cursor+2u*j,second[second_count++]);
    cursor+=2u*groups[i];
  }
  assert(second_count==64u);
  st=0x93CCu;
  for(unsigned i=0;i<70u;i++){
    leaf[leaf_count++]=st;
    st+=4u*(i<38u?7u:6u);
  }
  assert(st==0x9AF4u);
  st=0x9B82u;
  for(unsigned i=0;i<43u;i++){
    leaf[leaf_count++]=st;
    st+=4u*(i<36u?6u:5u);
  }
  assert(st==0x9F6Eu);
  st=0xAB1Fu;
  for(unsigned i=0;i<16u;i++){
    leaf[leaf_count++]=st;
    unsigned n=i<13u?10u:11u;
    for(unsigned j=0;j<n;j++){wr(st+7u*j,(i+j)&3u);}
    st+=7u*n;
    wr(st,0xFFu);st++;
  }
  assert(st==0xAFA4u);
  for(unsigned i=0;i<6u;i++)leaf[leaf_count++]=leaf[i];
  assert(leaf_count==135u);
  unsigned source=0;
  for(unsigned i=0;i<64u;i++){
    unsigned n=2u+(i<2u || (i>=42u && i<47u));
    for(unsigned j=0;j<n;j++)word(second[i]+2u*j,leaf[source++]);
  }
  assert(source==135u);
}
int main(void){
  CviiiRoomCatalog0657 result,keep;
  CviiiRoomLeaf0657 leaf,saved;
  artificial();
  assert(cviii_room_catalog_0657(b,16384,&result)==1);
  assert(result.stages==15u && result.tier2==64u && result.leaf_count==135u);
  assert(result.cells4==709u && result.records7==163u && result.terminators7==16u);
  for(unsigned i=0;i<135u;i++){
    assert(cviii_room_catalog_find_0657(&result,result.leaves[i].stage,
        result.leaves[i].second,result.leaves[i].third,&leaf)==1);
    assert(!memcmp(&leaf,&result.leaves[i],sizeof leaf));
    assert(leaf.root<leaf.end_exclusive && (leaf.width==4u||leaf.width==7u));
  }
  memset(&leaf,0xA5,sizeof leaf);saved=leaf;
  assert(cviii_room_catalog_find_0657(&result,15,0,0,&leaf)==0);
  assert(!memcmp(&leaf,&saved,sizeof leaf));
  memset(&result,0xA5,sizeof result);keep=result;
  assert(cviii_room_catalog_0657(b,16383,&result)==-1);
  assert(!memcmp(&result,&keep,sizeof result));
  assert(cviii_room_catalog_0657(NULL,16384,&result)==-1);
  assert(!memcmp(&result,&keep,sizeof result));
  assert(cviii_room_catalog_0657(b,16384,NULL)==-1);
  unsigned bad_offsets[]={0x92AEu,0x9320u,0xAB1Fu,0xAFA3u};
  for(unsigned i=0;i<4u;i++){
    unsigned cpu=bad_offsets[i],value=rd(cpu);
    wr(cpu,value==0xFFu?0u:0xFFu);
    assert(cviii_room_catalog_0657(b,16384,&result)==-1);
    assert(!memcmp(&result,&keep,sizeof result));
    wr(cpu,value);
  }
  assert(cviii_room_catalog_0657(b,16384,&result)==1);
  return 0;
}
