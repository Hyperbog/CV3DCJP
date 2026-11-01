/* JP0647: deterministic, mono signed-16 presentation voice merger.
 * No sample generator is included; sources cannot be invented from a
 * combined NES waveform. Exclusively host-owned virtual PCM callbacks. */
#include "cviii_virtual_audio_0647.h"
#include <limits.h>
#include <string.h>

static CviiiVirtualVoice *bank(CviiiVirtualAudio *m,CviiiVirtualBus b,
                                size_t *length)
{
    if(!m || !length) return NULL;
    if(b==CVIII_BUS_MUSIC) {
        *length=CVIII_MUSIC_VOICES;
        return m->music;
    }
    if(b==CVIII_BUS_SFX) {
        *length=CVIII_SFX_VOICES;
        return m->sfx;
    }
    return NULL;
}
void cviii_virtual_audio_init(CviiiVirtualAudio *m)
{
    if(m) memset(m,0,sizeof(*m));
}
void cviii_virtual_audio_clear(CviiiVirtualAudio *m)
{
    if(!m)return;
    memset(m->music,0,sizeof(m->music));
    memset(m->sfx,0,sizeof(m->sfx));
}
uint64_t cviii_virtual_audio_play(CviiiVirtualAudio *m,CviiiVirtualBus b,
                                  CviiiVirtualSample callback,void *ctx)
{
    size_t n=0;
    CviiiVirtualVoice *v=bank(m,b,&n);
    if(!v || !callback) return 0;
    if(m->next_id==UINT64_MAX) {++m->refused_voices;return 0;}
    for(size_t i=0;i<n;++i) {
        if(!v[i].sample) {
            const uint64_t id=++m->next_id;
            v[i]=(CviiiVirtualVoice){callback,ctx,id};
            return id;
        }
    }
    ++m->refused_voices;
    return 0;
}
bool cviii_virtual_audio_stop(CviiiVirtualAudio *m,uint64_t id)
{
    if(!m || !id)return false;
    for(unsigned b=0;b<2u;++b) {
        size_t n=0;
        CviiiVirtualVoice *v=bank(m,(CviiiVirtualBus)b,&n);
        for(size_t i=0;i<n;++i)if(v[i].sample && v[i].id==id) {
            v[i]=(CviiiVirtualVoice){0};
            return true;
        }
    }
    return false;
}
void cviii_virtual_audio_stop_bus(CviiiVirtualAudio *m,CviiiVirtualBus b)
{
    size_t n=0;
    CviiiVirtualVoice *v=bank(m,b,&n);
    if(v)memset(v,0,n*sizeof(*v));
}
size_t cviii_virtual_audio_count(const CviiiVirtualAudio *m,CviiiVirtualBus b)
{
    size_t n=0,result=0;
    CviiiVirtualVoice *v=bank((CviiiVirtualAudio *)m,b,&n);
    if(!v)return 0;
    for(size_t i=0;i<n;++i)result+=(v[i].sample!=NULL);
    return result;
}
static int64_t bus_sample(CviiiVirtualVoice *v,size_t n,CviiiVirtualAudio *m)
{
    int64_t sum=0;
    for(size_t i=0;i<n;++i) {
        if(!v[i].sample)continue;
        int16_t value=0;
        if(v[i].sample(v[i].context,&value)==1)sum+=(int64_t)value;
        else {
            v[i]=(CviiiVirtualVoice){0};
            ++m->completed_voices;
        }
    }
    return sum;
}
bool cviii_virtual_audio_render(CviiiVirtualAudio *m,
                                const CviiiAudioLevels *levels,
                                int16_t *out,size_t samples)
{
    if(!m || !levels || !out || samples>CVIII_AUDIO_MAX_BLOCK ||
       levels->master>100u || levels->music>100u || levels->sfx>100u ||
       m->produced_samples>UINT64_MAX-samples)
        return false;
    for(size_t i=0;i<samples;++i) {
        /* Regardless of gain (including 0%), advance every active source.
         * Otherwise muted sound would resume at the wrong note/event. */
        const int64_t music=bus_sample(m->music,CVIII_MUSIC_VOICES,m);
        const int64_t sfx=bus_sample(m->sfx,CVIII_SFX_VOICES,m);
        /* Accumulate and scale BEFORE clipping: large polyphony at
         * low Master must not clip prematurely. Maximum <=32*32768. */
        const int64_t mixed=((music*(int64_t)levels->music+
                              sfx*(int64_t)levels->sfx)/100)*
                              (int64_t)levels->master/100;
        const int64_t limited=mixed>INT16_MAX ? INT16_MAX :
                              mixed<INT16_MIN ? INT16_MIN : mixed;
        out[i]=(int16_t)limited;
    }
    m->produced_samples+=(uint64_t)samples;
    return true;
}
