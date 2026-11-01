/* ROM-/SDL-free tests for registry, 1% integer steps, duplicate IDs. */
#include "cviii_menu_registry_0620.h"
#include "cviii_audio_levels_0618.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {CviiiAudioLevels *levels;CviiiVolumeChannel channel;} Bind;
static int get(void *user,int *out)
{
    Bind *b=(Bind *)user;
    *out=cviii_audio_levels_get(b->levels,b->channel);
    return *out>=0;
}
static int set(void *user,int value)
{
    Bind *b=(Bind *)user;
    return cviii_audio_levels_set(b->levels,b->channel,value);
}
int main(void)
{
    CviiiMenuRegistry r={0};
    CviiiAudioLevels volumes=CVIII_AUDIO_LEVELS_DEFAULT;
    Bind bindings[3]={{&volumes,CVIII_VOLUME_MASTER},
                      {&volumes,CVIII_VOLUME_MUSIC},
                      {&volumes,CVIII_VOLUME_SFX}};
    const char *keys[]={"cviii.audio.master","cviii.audio.music","cviii.audio.sfx"};
    for(int i=0;i<3;++i){
        CviiiMenuSetting row={keys[i],"Audio","Volume",
                              i?"Stored pending source separation":"Master PCM",
                              0,100,1,&bindings[i],get,set};
        assert(cviii_menu_register(&r,&row));
        assert(!cviii_menu_register(&r,&row));
    }
    assert(r.count==3u);
    int value=-1;
    assert(cviii_menu_get(&r,keys[0],&value)&&value==100);
    assert(cviii_menu_set(&r,keys[0],99));
    assert(cviii_menu_get(&r,keys[0],&value)&&value==99);
    assert(cviii_menu_set(&r,keys[0],0));
    assert(cviii_menu_set(&r,keys[1],37));
    assert(cviii_menu_set(&r,keys[2],1));
    assert(volumes.master==0 && volumes.music==37 && volumes.sfx==1);
    assert(!cviii_menu_set(&r,keys[0],101));
    assert(!cviii_menu_set(&r,keys[0],-1));
    assert(!cviii_menu_set(&r,"cviii.missing",10));
    assert(!cviii_menu_get(&r,"cviii.missing",&value));
    CviiiMenuSetting bad={"cyc.override","Other","Bad","Reserved",
                          0,100,1,&bindings[0],get,set};
    assert(!cviii_menu_register(&r,&bad));
    bad.key="bad uppercase";
    assert(!cviii_menu_register(&r,&bad));
    bad.key="other.config";
    bad.step=0;
    assert(!cviii_menu_register(&r,&bad));
    bad.step=2;
    assert(cviii_menu_register(&r,&bad));
    assert(!cviii_menu_set(&r,"other.config",1));
    assert(cviii_menu_set(&r,"other.config",2));
    cviii_menu_seal(&r);
    assert(!cviii_menu_register(&r,&bad));
    puts("PASS JP0620 bounded namespaced registry, 1% menu edits and sealing");
    return 0;
}
