/* JP0622 ROM-free C11 behavioral checks: open / close / 1% / no leakage. */
#include "cviii_audio_menu_state_0622.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    CviiiAudioMenuState menu={0};
    CviiiAudioLevels levels=CVIII_AUDIO_LEVELS_DEFAULT;
    assert(!cviii_audio_menu_state_toggle(NULL,1,1));
    assert(!cviii_audio_menu_state_toggle(&menu,0,1));
    assert(!cviii_audio_menu_state_toggle(&menu,1,0));
    assert(!menu.open && !menu.selected);
    assert(!cviii_audio_menu_state_move(&menu,&levels,2));
    assert(!cviii_audio_menu_state_move(NULL,&levels,2));
    assert(cviii_audio_menu_state_toggle(&menu,1,1));
    assert(menu.open && menu.selected==0u);
    assert(!cviii_audio_menu_state_move(&menu,&levels,-1));
    assert(menu.selected==2u); /* Up wraps from Master to SFX */
    assert(cviii_audio_menu_state_move(&menu,&levels,-2));
    assert(levels.sfx==99u && levels.master==100u);
    assert(!cviii_audio_menu_state_move(&menu,&levels,1));
    assert(menu.selected==0u); /* Down wraps from SFX to Master */
    assert(cviii_audio_menu_state_move(&menu,&levels,-2));
    assert(levels.master==99u);
    assert(!cviii_audio_menu_state_move(&menu,&levels,1));
    assert(menu.selected==1u);
    assert(cviii_audio_menu_state_move(&menu,&levels,-2));
    assert(levels.music==99u);
    assert(!cviii_audio_menu_state_move(&menu,&levels,0));
    assert(levels.music==99u);
    assert(cviii_audio_menu_state_toggle(&menu,1,1));
    assert(!menu.open);
    assert(!cviii_audio_menu_state_move(&menu,&levels,2));
    assert(levels.music==99u); /* no gameplay input leakage after close */
    assert(cviii_audio_menu_state_toggle(&menu,1,1));
    assert(menu.selected==0u); /* deterministic re-entry */
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,0));
    assert(!cviii_audio_menu_state_move(&menu,&levels,-2));
    assert(levels.master==0u);
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,100));
    assert(!cviii_audio_menu_state_move(&menu,&levels,2));
    assert(levels.master==100u);
    assert(cviii_audio_menu_state_move(&menu,&levels,-2));
    assert(levels.master==99u);
    menu.selected=3u;
    assert(!cviii_audio_menu_state_move(&menu,&levels,-2));
    assert(levels.master==99u);
    cviii_audio_menu_state_close(&menu); /* state load / edition switch */
    assert(!menu.open);
    cviii_audio_menu_state_close(NULL);
    assert(!cviii_audio_menu_state_toggle(&menu,0,0)); /* cannot open */
    menu.open=1; /* edition switched while modal was open */
    assert(cviii_audio_menu_state_toggle(&menu,0,0)); /* still can close */
    assert(!menu.open);
    puts("PASS JP0622 isolated Enhanced F6 menu state / volume boundaries");
    return 0;
}
