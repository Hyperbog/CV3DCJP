/* JP0618 synthetic host-only tests: 1% steps, persistence, PCM gain. */
#include "cviii_audio_levels_0618.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    CviiiAudioLevels levels={0},restored={0};
    uint8_t v=255u;
    cviii_audio_levels_defaults(&levels);
    cviii_audio_levels_defaults(&restored);
    assert(levels.master==100 && levels.music==100 && levels.sfx==100);
    assert(cviii_audio_levels_step(&levels,CVIII_VOLUME_MASTER,-1));
    assert(levels.master==99u);
    assert(cviii_audio_levels_step(&levels,CVIII_VOLUME_MASTER,1));
    assert(levels.master==100u);
    assert(cviii_audio_levels_step(&levels,CVIII_VOLUME_MASTER,1));
    assert(levels.master==100u);
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,0));
    assert(cviii_audio_levels_step(&levels,CVIII_VOLUME_MASTER,-1));
    assert(levels.master==0);
    assert(!cviii_audio_levels_step(&levels,CVIII_VOLUME_MASTER,2));
    assert(!cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,101));
    assert(!cviii_audio_levels_set(&levels,(CviiiVolumeChannel)44,20));
    assert(!cviii_audio_levels_parse("101",&v));
    assert(!cviii_audio_levels_parse("-1",&v));
    assert(!cviii_audio_levels_parse(" 1",&v));
    assert(!cviii_audio_levels_parse("1 ",&v));
    assert(!cviii_audio_levels_parse("1.5",&v));
    assert(!cviii_audio_levels_parse("1\n",&v));
    assert(!cviii_audio_levels_parse("",&v));
    assert(!cviii_audio_levels_parse("1000",&v));
    assert(cviii_audio_levels_parse("01",&v) && v==1);
    assert(cviii_audio_levels_parse("100",&v) && v==100);
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MUSIC,63));
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_SFX,27));
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,50));

    int16_t sample[]={INT16_MIN,-101,-1,0,1,101,INT16_MAX};
    cviii_audio_levels_master_pcm(&levels,sample,sizeof sample/sizeof sample[0]);
    assert(sample[0]==-16384 && sample[1]==-50 && sample[2]==0);
    assert(sample[3]==0 && sample[4]==0 && sample[5]==50);
    assert(sample[6]==16383);
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,100));
    int16_t exact[]={INT16_MIN,-123,0,123,INT16_MAX};
    int16_t old[5];
    memcpy(old,exact,sizeof old);
    cviii_audio_levels_master_pcm(&levels,exact,5);
    assert(!memcmp(old,exact,sizeof old));
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,0));
    cviii_audio_levels_master_pcm(&levels,exact,5);
    for(unsigned i=0;i<5;i++) assert(exact[i]==0);
    assert(levels.music==63 && levels.sfx==27); /* not applied to mixed PCM */

    FILE *f=tmpfile();
    assert(f);
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,73));
    cviii_audio_levels_save(&levels,f);
    rewind(f);
    char line[96],key[32],value[32];
    for(unsigned i=0;i<3;i++){
        assert(fgets(line,sizeof line,f));
        assert(sscanf(line,"%31s = %31s",key,value)==2);
        assert(cviii_audio_levels_load(&restored,key,value));
    }
    assert(!fgets(line,sizeof line,f));
    fclose(f);
    assert(restored.master==73 && restored.music==63 && restored.sfx==27);
    assert(!cviii_audio_levels_load(&restored,"MusicVolume","999"));
    assert(restored.music==63);
    assert(!cviii_audio_levels_load(&restored,"UnknownKey","23"));
    puts("PASS JP0618 1% audio settings, strict persistence, master PCM gain");
    return 0;
}
