/* One marker is a boundary BEFORE the next event of each existing CSV. */
#include "cviii_audio_session_0614.h"
#include <inttypes.h>
#include <string.h>

static int valid_id(const char *p)
{
    if (!p || strlen(p)!=32) return 0;
    for (unsigned i=0;i<32;i++)
        if (!((p[i]>='0' && p[i]<='9') ||
              (p[i]>='a' && p[i]<='f'))) return 0;
    return 1;
}
int cviii_audio_session_open(CviiiAudioSession *s,const char *path,const char *id)
{
    if (!s || !path || !*path || !valid_id(id) || s->output) return 0;
    FILE *f=fopen(path,"w");
    if (!f) return 0;
    memset(s,0,sizeof(*s));
    memcpy(s->id,id,33);
    s->output=f;
    fputs("session_id,epoch,event,cycle,request_rows,register_rows\n",f);
    return 1;
}
void cviii_audio_session_mark(CviiiAudioSession *s,const char *event,
    uint64_t cycle,uint64_t requests,uint64_t writes,
    FILE *request_csv,FILE *register_csv)
{
    if (!s || !s->output || !request_csv || !register_csv) return;
    if (strcmp(event,"power_on") && strcmp(event,"state_loaded")) return;
    if (s->epochs >= 10000u || requests<s->prev_requests || writes<s->prev_writes) return;
    /* The two original logs remain authoritative; flush before marking. */
    fflush(request_csv);
    fflush(register_csv);
    fprintf(s->output,"%s,%u,%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
        s->id,(unsigned)s->epochs,event,cycle,requests,writes);
    fflush(s->output);
    ++s->epochs;
    s->prev_requests=requests;
    s->prev_writes=writes;
}
void cviii_audio_session_close(CviiiAudioSession *s)
{
    if (!s) return;
    if (s->output) fclose(s->output);
    s->output=NULL;
}
