/* ROM-free real game-object/OAM trace contract and rotation evidence. */
#include "cviii_object_oam_trace_0609.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    CviiiObjectOamTrace t={0};
    uint8_t ram[0x800]={0}, oam[256];
    FILE *f=tmpfile();
    if(!f) return 1;
    /* tmpfile bypasses the path-opening/header wrapper; seed its column
     * prefix here and focus this test on raw frame values and rotation. */
    fputs("frame,phase,phase_delta,active_objects,visible_oam\n",f);
    t.out=f; t.prev_phase=-1;
    memset(oam,0xf4,sizeof(oam));
    ram[0x22]=0x44; ram[0x0400]=2; ram[0x041c]=100;
    ram[0x0438]=40; ram[0x0454]=3; ram[0x0470]=0;
    ram[0x048c]=8; ram[0x04a8]=1;
    ram[0x0401]=4;ram[0x0471]=0x80; /* suppressed even though frame present */
    oam[0]=90;oam[1]=7;oam[2]=1;oam[3]=41;
    cviii_object_oam_trace_frame(&t,ram,oam);
    ram[0x22]=(uint8_t)(ram[0x22]+0x44);
    oam[4]=89;
    cviii_object_oam_trace_frame(&t,ram,oam);
    if(t.frames!=2||t.active_objects!=2||t.visible_sprites!=3||
       t.phase_moves!=1||t.phase_44_moves!=1||t.full_oam_frames!=0) return 2;
    fflush(f);rewind(f);
    char line[2048];
    if(!fgets(line,sizeof(line),f) || !strstr(line,"frame,phase,phase_delta,"))return 3;
    if(!fgets(line,sizeof(line),f) || !strstr(line,"1,68,-1,1,1,"))return 4;
    if(!fgets(line,sizeof(line),f) || !strstr(line,"2,136,68,1,2,"))return 5;
    if(fgets(line,sizeof(line),f))return 6;
    fclose(f);
    puts("PASS JP0609 game object / OAM shadow, suppressed actors and +$44 rotation");
    return 0;
}
