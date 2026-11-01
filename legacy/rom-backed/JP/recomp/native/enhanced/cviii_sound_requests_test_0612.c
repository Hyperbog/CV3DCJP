/* Synthetic-only sound-entry logger test. */
#include "cviii_sound_requests_0612.h"
#include <stdio.h>
#include <string.h>
int main(void)
{
    const char *path="jp0612_synthetic_requests.csv";
    CviiiSoundRequests r={0};
    if (!cviii_sound_requests_open(&r,path)) return 1;
    if (cviii_sound_requests_open(&r,path)) return 2;
    cviii_sound_requests_observe(123,0xE248,0x6B,1,2,0xFD,0x24,0x0A,0x90,0xB5);
    cviii_sound_requests_observe(124,0xE249,0x6B,1,2,0xFD,0x24,0x0A,0x90,0xB5);
    cviii_sound_requests_observe(125,0xE249,0x10,3,4,0xFE,0x20,0x0C,0x00,0x00);
    if (r.requests != 2 || r.dropped) return 3;
    cviii_sound_requests_close(&r);
    FILE *f=fopen(path,"r");
    if (!f) return 4;
    char line[200]; int lines=0,first=0,second=0;
    while (fgets(line,sizeof(line),f)) {
        ++lines;
        if (strstr(line,"124,E249,6B,01,02,FD,24,0A,90,B5")) first=1;
        if (strstr(line,"125,E249,10,03,04,FE,20,0C,00,00")) second=1;
    }
    fclose(f); remove(path);
    if (lines!=3 || !first || !second) return 5;
    puts("PASS JP0612 original $E249 request logger: owner-local and nonmutating");
    return 0;
}
