/* JP0647: deterministic synthetic "level music plus simultaneous SFX"
 * across independent audio callback sizes and host UI gain changes. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_virtual_audio_0647.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TOTAL 10000u
#define SEGMENT 2000u
typedef struct {int16_t volume;unsigned remaining;unsigned calls;} Tone;
static int next(void *ptr,int16_t *s)
{
    Tone *x=(Tone *)ptr;
    ++x->calls;
    if(!x->remaining)return 0;
    *s=x->volume;
    --x->remaining;
    return 1;
}
static uint64_t scene(int16_t *out,size_t block)
{
    CviiiVirtualAudio m;
    cviii_virtual_audio_init(&m);
    CviiiAudioLevels gains=CVIII_AUDIO_LEVELS_DEFAULT;
    Tone song1={1300,TOTAL,0}, song2={-200,TOTAL,0};
    Tone fx1={7000,3000,0},fx2={-4500,4000,0},fx3={2500,3000,0};
    uint64_t musicA=cviii_virtual_audio_play(&m,CVIII_BUS_MUSIC,next,&song1);
    uint64_t musicB=cviii_virtual_audio_play(&m,CVIII_BUS_MUSIC,next,&song2);
    assert(musicA && musicB);
    for(size_t frame=0;frame<5;frame++) {
        /* Synthetic stage cues: jumping, collision, door all overlap. */
        if(frame==1)assert(cviii_virtual_audio_play(&m,CVIII_BUS_SFX,next,&fx1));
        if(frame==2)assert(cviii_virtual_audio_play(&m,CVIII_BUS_SFX,next,&fx2));
        if(frame==3) {
            assert(cviii_virtual_audio_play(&m,CVIII_BUS_SFX,next,&fx3));
            assert(cviii_audio_levels_set(&gains,CVIII_VOLUME_MUSIC,35));
            assert(cviii_audio_levels_set(&gains,CVIII_VOLUME_SFX,25));
        }
        if(frame==4)assert(cviii_audio_levels_set(&gains,CVIII_VOLUME_MASTER,0));
        size_t offset=0;
        while(offset<SEGMENT) {
            const size_t n=(SEGMENT-offset)<block ? (SEGMENT-offset) : block;
            assert(cviii_virtual_audio_render(&m,&gains,out+frame*SEGMENT+offset,n));
            offset+=n;
        }
        assert(cviii_virtual_audio_count(&m,CVIII_BUS_MUSIC)==2);
    }
    assert(song1.calls==TOTAL && song2.calls==TOTAL);
    assert(fx1.calls==3001 && fx2.calls==4001 && fx3.calls==3001);
    assert(m.produced_samples==TOTAL);
    assert(out[0]==1100);
    assert(out[SEGMENT]==8100);
    assert(out[2*SEGMENT]==3600); /* effect1+effect2 + unbroken music */
    assert(out[3*SEGMENT]==((1100*35+(-4500+2500)*25)/100));
    for(size_t i=4*SEGMENT;i<TOTAL;i++)assert(out[i]==0);
    /* No artificial 'song restart' or SFX voice stealing occurred. */
    assert(!cviii_virtual_audio_stop(&m,0));
    assert(cviii_virtual_audio_count(&m,CVIII_BUS_MUSIC)==2);
    return m.produced_samples;
}
int main(void)
{
    int16_t *single=calloc(TOTAL,sizeof(int16_t));
    int16_t *medium=calloc(TOTAL,sizeof(int16_t));
    int16_t *tiny=calloc(TOTAL,sizeof(int16_t));
    assert(single && medium && tiny);
    assert(scene(single,SEGMENT)==TOTAL);
    assert(scene(medium,257u)==TOTAL);
    assert(scene(tiny,1u)==TOTAL);
    assert(memcmp(single,medium,TOTAL*sizeof(int16_t))==0);
    assert(memcmp(single,tiny,TOTAL*sizeof(int16_t))==0);
    free(single);free(medium);free(tiny);
    puts("PASS JP0647 synthetic song+SFX scene: 3 simultaneous events, 3 callback sizes, continuous music, gain changes, mute");
    return 0;
}
