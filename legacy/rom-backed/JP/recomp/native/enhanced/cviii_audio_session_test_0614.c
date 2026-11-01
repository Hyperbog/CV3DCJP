/* Synthetic-only JP0614 session/epoch marker contract. */
#include "cviii_audio_session_0614.h"
#include <stdio.h>
#include <string.h>
int main(void)
{
    CviiiAudioSession s={0};
    FILE *requests=tmpfile(),*registers=tmpfile();
    const char *id="0123456789abcdef0123456789abcdef";
    const char *path="jp0614_synthetic_epochs.csv";
    if (!requests || !registers) return 1;
    if (cviii_audio_session_open(&s,path,"not-a-guid")) return 2;
    if (!cviii_audio_session_open(&s,path,id)) return 3;
    if (cviii_audio_session_open(&s,path,id)) return 4;
    cviii_audio_session_mark(&s,"power_on",0,0,0,requests,registers);
    cviii_audio_session_mark(&s,"state_loaded",300,2,3,requests,registers);
    cviii_audio_session_mark(&s,"state_loaded",40,4,6,requests,registers);
    cviii_audio_session_mark(&s,"state_loaded",80,3,7,requests,registers);
    if(s.epochs!=3 || s.prev_requests!=4 || s.prev_writes!=6) return 5;
    cviii_audio_session_close(&s);
    FILE *f=fopen(path,"r");
    if(!f)return 6;
    char line[180];int lines=0,rolled=0;
    while(fgets(line,sizeof(line),f)){
        lines++;
        if(strstr(line,",2,state_loaded,40,4,6"))rolled=1;
    }
    fclose(f);remove(path);fclose(requests);fclose(registers);
    if(lines!=4 || !rolled) return 7;
    puts("PASS JP0614 session GUID and row-indexed save/load epoch boundaries");
    return 0;
}
