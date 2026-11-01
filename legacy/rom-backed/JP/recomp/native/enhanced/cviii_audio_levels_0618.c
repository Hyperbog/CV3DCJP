/* JP0618: host PCM gain, no NES memory, register writes or PCM producers. */
#include "cviii_audio_levels_0618.h"
#include <string.h>

static uint8_t *slot(CviiiAudioLevels *s, CviiiVolumeChannel channel)
{
    if (!s) return NULL;
    switch (channel) {
    case CVIII_VOLUME_MASTER: return &s->master;
    case CVIII_VOLUME_MUSIC: return &s->music;
    case CVIII_VOLUME_SFX: return &s->sfx;
    default: return NULL;
    }
}
void cviii_audio_levels_defaults(CviiiAudioLevels *s)
{
    if (!s) return;
    s->master=100u;
    s->music=100u;
    s->sfx=100u;
}
int cviii_audio_levels_parse(const char *value, uint8_t *out)
{
    unsigned result=0u;
    size_t n=0;
    if (!value || !*value || !out) return 0;
    for (; value[n]; ++n) {
        if (n>=3u || value[n]<'0' || value[n]>'9') return 0;
        result=result*10u+(unsigned)(value[n]-'0');
    }
    if (result>100u) return 0;
    *out=(uint8_t)result;
    return 1;
}
int cviii_audio_levels_set(CviiiAudioLevels *s, CviiiVolumeChannel channel,
                           int percent)
{
    uint8_t *target=slot(s,channel);
    if (!target || percent<0 || percent>100) return 0;
    *target=(uint8_t)percent;
    return 1;
}
int cviii_audio_levels_get(const CviiiAudioLevels *s,
                           CviiiVolumeChannel channel)
{
    const uint8_t *target=slot((CviiiAudioLevels *)s,channel);
    return target ? (int)*target : -1;
}
int cviii_audio_levels_step(CviiiAudioLevels *s,
                            CviiiVolumeChannel channel, int direction)
{
    int current=cviii_audio_levels_get(s,channel);
    if (current<0 || (direction!=1 && direction!=-1)) return 0;
    if (current+direction<0 || current+direction>100) return 1;
    return cviii_audio_levels_set(s,channel,current+direction);
}
int cviii_audio_levels_load(CviiiAudioLevels *s,const char *key,
                             const char *value)
{
    uint8_t parsed=0;
    CviiiVolumeChannel channel;
    if (!s || !key) return 0;
    if (!strcmp(key,"MasterVolume")) channel=CVIII_VOLUME_MASTER;
    else if (!strcmp(key,"MusicVolume")) channel=CVIII_VOLUME_MUSIC;
    else if (!strcmp(key,"SoundEffectsVolume")) channel=CVIII_VOLUME_SFX;
    else return 0;
    if (!cviii_audio_levels_parse(value,&parsed)) return 0;
    return cviii_audio_levels_set(s,channel,(int)parsed);
}
void cviii_audio_levels_save(const CviiiAudioLevels *s,FILE *output)
{
    if (!s || !output) return;
    fprintf(output,"MasterVolume = %u\nMusicVolume = %u\nSoundEffectsVolume = %u\n",
            (unsigned)s->master,(unsigned)s->music,(unsigned)s->sfx);
}
void cviii_audio_levels_master_pcm(const CviiiAudioLevels *s,
                                    int16_t *samples,size_t count)
{
    if (!s || !samples || s->master==100u) return;
    const int32_t percent=(int32_t)s->master;
    for (size_t i=0;i<count;i++) {
        const int32_t raw=(int32_t)samples[i];
        samples[i]=(int16_t)(raw*percent/100);
    }
}
