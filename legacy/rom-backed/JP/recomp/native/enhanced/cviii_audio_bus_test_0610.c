/* JP0610 sound channel bus tracing, no cartridge data or audio changes. */
#include "cviii_audio_bus_0610.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    CviiiAudioBus t={0};
    FILE *probe=tmpfile();
    if(!probe)return 1;
    fclose(probe);
    const char *path="jp0610_synthetic_audio_trace.csv";
    if(!cviii_audio_bus_open(&t,path))return 2;
    if(cviii_audio_bus_open(&t,path))return 3;
    cviii_audio_bus_observe(100u,0x4000u,0x21u);
    cviii_audio_bus_observe(200u,0x4000u,0x81u);
    cviii_audio_bus_observe(300u,0x4004u,0x44u);
    cviii_audio_bus_observe(400u,0x9000u,0x07u);
    cviii_audio_bus_observe(500u,0x9003u,0x02u);
    cviii_audio_bus_observe(600u,0xB002u,0x83u);
    cviii_audio_bus_observe(700u,0xB003u,0xFFu); /* mapper CHR CTRL, NOT SFX */
    cviii_audio_bus_observe(800u,0x4014u,0x01u); /* OAM DMA, NOT SFX */
    cviii_audio_bus_observe(900u,0x4015u,0x1Fu);
    cviii_audio_bus_observe(1000u,0x4017u,0xC0u);
    if(t.writes!=8 || t.rapid_rewrites!=1 || t.dropped)return 4;
    cviii_audio_bus_close(&t);
    FILE *f=fopen(path,"r");
    if(!f)return 5;
    char line[200]; int lines=0,found=0,vrc=0;
    while(fgets(line,sizeof(line),f)){
        ++lines;
        if(strstr(line,"200,2a03,pulse1,4000,81,21,100,1"))found=1;
        if(strstr(line,"600,vrc6,saw,B002,83"))vrc=1;
    }
    fclose(f);remove(path);
    if(lines!=9 || !found || !vrc)return 6;
    puts("PASS JP0610 music/SFX conflict diagnosis: 2A03+VRC6 CPU writes preserved");
    return 0;
}
