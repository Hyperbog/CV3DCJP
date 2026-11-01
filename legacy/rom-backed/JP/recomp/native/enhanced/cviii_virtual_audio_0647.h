/* JP0647: isolated, host-only independent music/SFX PCM voice mixer.
 * No CPU/APU/VRC6 emulation, original PCM substitution or implicit sources.
 * Callback memory belongs to caller and must outlive the voice. */
#ifndef CVIII_VIRTUAL_AUDIO_0647_H
#define CVIII_VIRTUAL_AUDIO_0647_H
#include "cviii_audio_levels_0618.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CVIII_MUSIC_VOICES 8u
#define CVIII_SFX_VOICES 24u
#define CVIII_AUDIO_MAX_BLOCK 65536u

typedef enum {
    CVIII_BUS_MUSIC = 0,
    CVIII_BUS_SFX = 1
} CviiiVirtualBus;
/* 1: valid sample including silence; 0: source has finished. A non-1
 * return removes that voice; callbacks may not reenter the same mixer. */
typedef int (*CviiiVirtualSample)(void *context, int16_t *sample);
typedef struct {
    CviiiVirtualSample sample;
    void *context;
    uint64_t id;
} CviiiVirtualVoice;
typedef struct {
    CviiiVirtualVoice music[CVIII_MUSIC_VOICES];
    CviiiVirtualVoice sfx[CVIII_SFX_VOICES];
    uint64_t next_id, produced_samples, completed_voices, refused_voices;
} CviiiVirtualAudio;

/* init once; clear on power/state-load or song reset. A clear preserves
 * monotonically increasing voice IDs to prevent stale handles killing
 * new voices after a save/load. This host presentation is NOT savestated. */
void cviii_virtual_audio_init(CviiiVirtualAudio *mixer);
void cviii_virtual_audio_clear(CviiiVirtualAudio *mixer);
/* Returns a nonzero unique handle on success; 0 on invalid/full/overflow.
 * There is NO priority stealing: SFX never displace music, and vice versa. */
uint64_t cviii_virtual_audio_play(CviiiVirtualAudio *mixer,
                                  CviiiVirtualBus bus,
                                  CviiiVirtualSample next, void *context);
bool cviii_virtual_audio_stop(CviiiVirtualAudio *mixer,uint64_t id);
void cviii_virtual_audio_stop_bus(CviiiVirtualAudio *mixer,CviiiVirtualBus bus);
size_t cviii_virtual_audio_count(const CviiiVirtualAudio *mixer,
                                 CviiiVirtualBus bus);
/* Render synthetic/provided VIRTUAL voices ONLY. Never feed the original
 * NESRecomp mixed PCM into this function or suppress original PCM before
 * source-validated real original voice producers are available. On error
 * returns false and leaves the output buffer unchanged. Output is mono. */
bool cviii_virtual_audio_render(CviiiVirtualAudio *mixer,
                                const CviiiAudioLevels *levels,
                                int16_t *out,size_t samples);
#endif
