/* JP0618 enhanced-only volume state, independent of emulated machine memory.
 * Final UI: three health-bar-art sliders, 0..100 in steps of 1.
 * Only master is connected to mixed PCM until source buses are separated. */
#ifndef CVIII_AUDIO_LEVELS_0618_H
#define CVIII_AUDIO_LEVELS_0618_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef enum {
    CVIII_VOLUME_MASTER=0,
    CVIII_VOLUME_MUSIC=1,
    CVIII_VOLUME_SFX=2
} CviiiVolumeChannel;
typedef struct {
    uint8_t master, music, sfx;
} CviiiAudioLevels;
#define CVIII_AUDIO_LEVELS_DEFAULT {100u,100u,100u}
void cviii_audio_levels_defaults(CviiiAudioLevels *settings);
int cviii_audio_levels_parse(const char *value, uint8_t *out);
int cviii_audio_levels_set(CviiiAudioLevels *settings,
                            CviiiVolumeChannel channel, int percent);
int cviii_audio_levels_step(CviiiAudioLevels *settings,
                             CviiiVolumeChannel channel, int direction);
int cviii_audio_levels_get(const CviiiAudioLevels *settings,
                            CviiiVolumeChannel channel);
int cviii_audio_levels_load(CviiiAudioLevels *settings,
                             const char *key,const char *value);
void cviii_audio_levels_save(const CviiiAudioLevels *settings,FILE *output);
void cviii_audio_levels_master_pcm(const CviiiAudioLevels *settings,
                                    int16_t *samples,size_t count);
#endif
