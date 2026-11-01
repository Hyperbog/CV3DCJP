/* JP0647: standalone exact production C11 virtual-voice PCM regression. */
#ifdef NDEBUG
#undef NDEBUG /* Assert-driven test fixtures MUST run on Windows Release. */
#endif
#include "cviii_virtual_audio_0647.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
typedef struct {int16_t value;unsigned left,called;} Source;
static int sample(void *opaque,int16_t *out)
{
    Source *s=(Source *)opaque;
    ++s->called;
    if(!s->left)return 0;
    --s->left;
    *out=s->value;
    return 1;
}
static void expect(int16_t *actual,const int16_t *expected,size_t n)
{
    for(size_t i=0;i<n;++i)assert(actual[i]==expected[i]);
}
int main(void)
{
    CviiiVirtualAudio m;
    cviii_virtual_audio_init(&m);
    CviiiAudioLevels levels={100,100,100};
    Source music={1000,8,0},sfx={200,3,0},next={-400,5,0};
    assert(!cviii_virtual_audio_play(NULL,CVIII_BUS_MUSIC,sample,&music));
    assert(!cviii_virtual_audio_play(&m,(CviiiVirtualBus)2,sample,&music));
    assert(!cviii_virtual_audio_play(&m,CVIII_BUS_SFX,NULL,&music));
    const uint64_t a=cviii_virtual_audio_play(&m,CVIII_BUS_MUSIC,sample,&music);
    const uint64_t b=cviii_virtual_audio_play(&m,CVIII_BUS_SFX,sample,&sfx);
    const uint64_t c=cviii_virtual_audio_play(&m,CVIII_BUS_SFX,sample,&next);
    assert(a && b && c && a!=b && b!=c);
    assert(cviii_virtual_audio_count(&m,CVIII_BUS_MUSIC)==1);
    assert(cviii_virtual_audio_count(&m,CVIII_BUS_SFX)==2);
    int16_t out[11]={0};
    assert(cviii_virtual_audio_render(&m,&levels,out,4));
    expect(out,(int16_t[]){800,800,800,600},4);
    assert(music.called==4 && sfx.called==4 && next.called==4);
    assert(cviii_virtual_audio_count(&m,CVIII_BUS_SFX)==1);
    levels.music=50;levels.sfx=25;levels.master=50;
    assert(cviii_virtual_audio_render(&m,&levels,out,3));
    /* t4: (1000*0.50-400*0.25)*.50=200;
     * t5..6: SFX reaches end; Music remains 250. */
    expect(out,(int16_t[]){200,250,250},3);
    assert(music.called==7 && next.called==6);
    assert(cviii_virtual_audio_count(&m,CVIII_BUS_SFX)==0);
    assert(cviii_virtual_audio_render(&m,&levels,out,2));
    assert(out[0]==250 && out[1]==0); /* final music note then finish */
    assert(cviii_virtual_audio_count(&m,CVIII_BUS_MUSIC)==0);
    assert(m.produced_samples==9 && m.completed_voices==3);

    /* A zero-volume bus still consumes samples; start SFX without
     * stealing, interrupting or decrementing any music voice. */
    cviii_virtual_audio_clear(&m);
    Source low={-1000,3,0},fx={9000,3,0};
    const uint64_t mhandle=cviii_virtual_audio_play(&m,CVIII_BUS_MUSIC,sample,&low);
    assert(mhandle>c);
    const uint64_t shandle=cviii_virtual_audio_play(&m,CVIII_BUS_SFX,sample,&fx);
    levels=(CviiiAudioLevels){100,100,0};
    assert(cviii_virtual_audio_render(&m,&levels,out,2));
    expect(out,(int16_t[]){-1000,-1000},2);
    assert(fx.called==2 && low.called==2);
    cviii_virtual_audio_stop_bus(&m,CVIII_BUS_SFX);
    assert(cviii_virtual_audio_count(&m,CVIII_BUS_SFX)==0);
    assert(cviii_virtual_audio_count(&m,CVIII_BUS_MUSIC)==1);
    assert(!cviii_virtual_audio_stop(&m,shandle));
    assert(cviii_virtual_audio_stop(&m,mhandle));
    assert(!cviii_virtual_audio_stop(&m,mhandle));

    /* Max polyphony never steals music/SFX, and handles can't be
     * accidentally reused following a loaded guest savestate. */
    cviii_virtual_audio_clear(&m);
    Source one={10000,999,0};
    uint64_t ids[CVIII_MUSIC_VOICES+CVIII_SFX_VOICES];
    for(unsigned i=0;i<CVIII_MUSIC_VOICES;++i)
        assert((ids[i]=cviii_virtual_audio_play(&m,CVIII_BUS_MUSIC,sample,&one)));
    for(unsigned i=0;i<CVIII_SFX_VOICES;++i)
        assert((ids[CVIII_MUSIC_VOICES+i]=cviii_virtual_audio_play(&m,CVIII_BUS_SFX,sample,&one)));
    assert(!cviii_virtual_audio_play(&m,CVIII_BUS_MUSIC,sample,&one));
    assert(!cviii_virtual_audio_play(&m,CVIII_BUS_SFX,sample,&one));
    assert(m.refused_voices==2);
    levels=(CviiiAudioLevels){10,100,100};
    assert(cviii_virtual_audio_render(&m,&levels,out,1));
    assert(out[0]==32000); /* avoid premature per-bus clipping */
    levels.master=100;
    assert(cviii_virtual_audio_render(&m,&levels,out,1));
    assert(out[0]==INT16_MAX); /* clip only after final gain */
    const uint64_t stale=ids[0];
    cviii_virtual_audio_clear(&m);
    assert(!cviii_virtual_audio_stop(&m,stale));
    assert(cviii_virtual_audio_play(&m,CVIII_BUS_MUSIC,sample,&one)>ids[31]);

    /* Invalid render inputs leave both voice progress and output intact. */
    out[0]=12345;
    const unsigned before=one.called;
    levels.music=101;
    assert(!cviii_virtual_audio_render(&m,&levels,out,1));
    assert(out[0]==12345 && one.called==before);
    levels.music=100;
    assert(!cviii_virtual_audio_render(&m,&levels,out,CVIII_AUDIO_MAX_BLOCK+1u));
    assert(out[0]==12345 && one.called==before);
    assert(!cviii_virtual_audio_render(&m,&levels,NULL,1));
    assert(!cviii_virtual_audio_render(NULL,&levels,out,1));
    assert(!cviii_virtual_audio_render(&m,NULL,out,1));
    assert(cviii_virtual_audio_count(&m,(CviiiVirtualBus)3)==0);
    /* Host ID rollover refuses admission rather than issuing a stale token. */
    cviii_virtual_audio_clear(&m);
    m.next_id=UINT64_MAX;
    assert(!cviii_virtual_audio_play(&m,CVIII_BUS_MUSIC,sample,&one));
    assert(m.refused_voices==3);
    puts("PASS JP0647 independent virtual PCM: 8+24 voices, two gains, tails, mute, clipping, reset, no stealing, Windows Release");
    return 0;
}
