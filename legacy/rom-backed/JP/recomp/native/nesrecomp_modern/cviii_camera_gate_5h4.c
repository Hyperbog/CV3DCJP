#include "cviii_camera_gate_5h4.h"
static void add(CviiiCameraSeed5H4 seeds[3],unsigned *n,unsigned pixel,unsigned how)
{
 if(*n>=3)return;
 for(unsigned i=0;i<*n;i++)if(seeds[i].pixel==pixel)return;
 seeds[*n]=(CviiiCameraSeed5H4){pixel,how};++*n;
}
unsigned cviii_camera_seeds_5h4(unsigned stage,unsigned area,unsigned room,
    int vertical,unsigned length,unsigned camera_ram,int ppu_scroll,
    CviiiCameraSeed5H4 seeds[3])
{
 if(!seeds||!length)return 0;
 unsigned n=0;
 /* Source-verified 0/0/0 -> 0/1/0, HORIZONTAL ONLY. A scroll
  * value is a candidate, not a persistent world coordinate. */
 if(stage==0&&area==1&&room==0&&!vertical&&ppu_scroll>=0&&ppu_scroll<512)
     add(seeds,&n,(unsigned)ppu_scroll%256u,CVIII_ANCHOR_PPU_SCROLL);
 if(camera_ram<length)add(seeds,&n,camera_ram,CVIII_ANCHOR_RAM);
 /* Allow a stationary/transition picture to be evaluated, but NEVER
  * accept it without on-screen pixel parity. */
 add(seeds,&n,0,CVIII_ANCHOR_ROOM_ORIGIN);
 return n;
}
const char *cviii_camera_gate_name_5h4(unsigned gate)
{
 static const char *const names[]={
 "NOT_EVALUATED","ASSETS_MISSING","RAM_MISSING","ROOM_UNKNOWN",
 "GRAPHICS_UNKNOWN","CHR_UNRESOLVED","SOURCE_INVALID",
 "CAMERA_MISMATCH","WORLD_UNPLACED","NO_MARGIN",
 "ATLAS_PRESENTED","ROOM_PRESENTED","FORCED_PPU",
 "PPU_CHECKED","PPU_REJECTED","LOCAL_PRESENTED",
 "LOCAL_CAMERA_MISMATCH","DUAL_PRESENTED","DYNAMIC_PRESENTED"};
 return gate<sizeof names/sizeof names[0]?names[gate]:"INVALID_GATE";
}
