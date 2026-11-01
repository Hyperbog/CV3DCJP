#include "cviii_room_catalog_0657.h"
#define CPU_BASE 0x8000u
static unsigned word(const uint8_t *b,unsigned cpu) {
  unsigned o=cpu-CPU_BASE;
  return (unsigned)b[o]|((unsigned)b[o+1u]<<8u);
}
static int in4(unsigned p) {
  return (p>=0x93CCu && p<0x9AF4u)||(p>=0x9B82u && p<0x9F6Eu);
}
int cviii_room_catalog_0657(const uint8_t *b,size_t len,CviiiRoomCatalog0657 *out) {
  CviiiRoomCatalog0657 r={0};
  unsigned stage_root[15], second_root[64], root4[113], root7[16];
  unsigned nr4=0,nr7=0, idx2=0,idx3=0,k,i,j,st,en,leaf;
  if (!b||!out||len!=16384u)return -1;
  for(i=0;i<15u;i++){
    st=word(b,0x92AEu+2u*i);
    stage_root[i]=st;
    if ((i&&st<=stage_root[i-1u])||
        !((st>=0x92CCu && st<0x9320u) || (st>=0x9AF4u && st<0x9B20u)))return -1;
  }
  if(stage_root[0]!=0x92CCu||stage_root[9]!=0x9AF4u)return -1;
  for(i=0;i<15u;i++){
    st=stage_root[i];
    en=i==8u?0x9320u:i==14u?0x9B20u:stage_root[i+1u];
    if(en<=st||(en-st)%2u)return -1;
    for(j=0;st+2u*j<en;j++){
      if(idx2>=64u)return -1;
      k=word(b,st+2u*j);
      if(idx2&&k<=second_root[idx2-1u])return -1;
      if(!((k>=0x9320u&&k<0x93CCu)||(k>=0x9B20u&&k<0x9B82u)))return -1;
      second_root[idx2++]=k;
    }
  }
  if(idx2!=64u||second_root[0]!=0x9320u||second_root[42]!=0x9B20u)return -1;
  idx2=0;
  for(i=0;i<15u;i++){
    st=stage_root[i];
    en=i==8u?0x9320u:i==14u?0x9B20u:stage_root[i+1u];
    for(j=0;st+2u*j<en;j++){
      unsigned sroot=second_root[idx2];
      unsigned send=idx2==41u?0x93CCu:idx2==63u?0x9B82u:second_root[idx2+1u];
      unsigned t;
      if(sroot>=send || (send-sroot)%2u)return -1;
      for(t=0;sroot+2u*t<send;t++){
        CviiiRoomLeaf0657 *dst;
        if(idx3>=135u)return -1;
        leaf=word(b,sroot+2u*t);
        dst=&r.leaves[idx3++];
        dst->stage=(uint8_t)i;dst->second=(uint8_t)j;dst->third=(uint8_t)t;
        dst->root=(uint16_t)leaf;
        if(in4(leaf))dst->width=4u;
        else if(leaf>=0xAB1Fu&&leaf<0xAFA4u)dst->width=7u;
        else return -1;
        /* Sorted de-duplicated roots across mixed third-level index order.
         * Structural offsets, not musical/gameplay object IDs. */
        if(dst->width==4u){
          for(k=0;k<nr4&&root4[k]!=leaf;k++);
          if(k==nr4){if(nr4>=113u)return -1;root4[nr4++]=leaf;}
        }else{
          for(k=0;k<nr7&&root7[k]!=leaf;k++);
          if(k==nr7){if(nr7>=16u)return -1;root7[nr7++]=leaf;}
        }
      }
      idx2++;
    }
  }
  if(idx2!=64u||idx3!=135u||nr4!=113u||nr7!=16u)return -1;
  /* Root ordering is by source address, not pointer traversal order. */
  for(i=0;i<nr4;i++)for(j=i+1u;j<nr4;j++)
    if(root4[j]<root4[i]){k=root4[i];root4[i]=root4[j];root4[j]=k;}
  for(i=0;i<nr7;i++)for(j=i+1u;j<nr7;j++)
    if(root7[j]<root7[i]){k=root7[i];root7[i]=root7[j];root7[j]=k;}
  if(root4[0]!=0x93CCu||root4[70]!=0x9B82u||root7[0]!=0xAB1Fu)return -1;
  for(i=0;i<nr4;i++){
    en= i==69u?0x9AF4u:i==112u?0x9F6Eu:root4[i+1u];
    if(en<=root4[i]||(en-root4[i])%4u)return -1;
    r.cells4=(uint16_t)(r.cells4+(en-root4[i])/4u);
  }
  for(i=0;i<nr7;i++){
    en=i==15u?0xAFA4u:root7[i+1u];
    if(en<=root7[i]+1u||(en-root7[i]-1u)%7u ||
       b[en-CPU_BASE-1u]!=0xFFu)return -1;
    for(st=root7[i];st<en-1u;st+=7u){
      if(b[st-CPU_BASE]>3u)return -1;
      r.records7++;
    }
    r.terminators7++;
  }
  if(r.cells4!=709u||r.records7!=163u||r.terminators7!=16u)return -1;
  for(i=0;i<135u;i++){
    unsigned *roots=r.leaves[i].width==4u?root4:root7;
    unsigned size=r.leaves[i].width==4u?nr4:nr7;
    for(j=0;j<size&&roots[j]!=r.leaves[i].root;j++);
    if(j==size)return -1;
    en=(j+1u<size)?roots[j+1u]:(size==nr4?0x9F6Eu:0xAFA4u);
    if(size==nr4 && j==69u)en=0x9AF4u;
    r.leaves[i].end_exclusive=(uint16_t)en;
  }
  r.leaf_count=135u;r.stages=15u;r.tier2=64u;
  *out=r;
  return 1;
}
int cviii_room_catalog_find_0657(const CviiiRoomCatalog0657 *c,
  uint8_t stage,uint8_t second,uint8_t third,CviiiRoomLeaf0657 *out) {
  unsigned i;
  if(!c||!out||c->leaf_count!=135u)return -1;
  for(i=0;i<135u;i++)if(c->leaves[i].stage==stage&&
     c->leaves[i].second==second&&c->leaves[i].third==third){
    *out=c->leaves[i];return 1;
  }
  return 0;
}
