#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_camera_gate_5h4.h"
#include <assert.h>
#include <string.h>
int main(void)
{
 CviiiCameraSeed5H4 s[3]={{0}};
 /* First verified edge: high RAM byte changes every transition frame,
  * but the measured PPU scroll is a valid alternative candidate. */
 assert(cviii_camera_seeds_5h4(0,1,0,0,256,1294,22,s)==2);
 assert(s[0].pixel==22&&s[0].source==CVIII_ANCHOR_PPU_SCROLL);
 assert(s[1].pixel==0&&s[1].source==CVIII_ANCHOR_ROOM_ORIGIN);
 assert(cviii_camera_seeds_5h4(0,1,0,0,256,22,22,s)==2);
 assert(s[0].pixel==22&&s[1].pixel==0);
 assert(cviii_camera_seeds_5h4(0,1,0,0,256,14,0,s)==2);
 assert(s[0].pixel==0&&s[1].pixel==14);
 assert(cviii_camera_seeds_5h4(0,1,1,0,256,2574,33,s)==1);
 assert(s[0].pixel==0); /* Unproven edges never gain a PPU anchor */
 assert(cviii_camera_seeds_5h4(0,0,0,0,768,380,120,s)==2);
 assert(s[0].pixel==380&&s[0].source==CVIII_ANCHOR_RAM);
 assert(cviii_camera_seeds_5h4(0,1,0,1,256,1,40,s)==2);
 assert(s[0].source==CVIII_ANCHOR_RAM);
 assert(!strcmp(cviii_camera_gate_name_5h4(CVIII_GATE_CAMERA_MISMATCH),"CAMERA_MISMATCH"));
 assert(cviii_camera_seeds_5h4(0,1,0,0,0,5,5,s)==0);
 return 0;
}
