/* JP0626 strict C11 proof of F6 standalone and nested Pause->Audio->Pause. */
#include "cviii_menu_stack_0626.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    CviiiStartMenu p={0};
    CviiiAudioMenuState a={0};
    CviiiAudioLevels levels=CVIII_AUDIO_LEVELS_DEFAULT;
    assert(cviii_start_defaults(&p));
    assert(!cviii_menu_stack_parent_active(&p,&a));
    assert(!cviii_menu_stack_enter_audio(&p,&a,1,1));
    assert(cviii_audio_menu_state_toggle(&a,1,1)); /* F6 from gameplay */
    assert(a.open && !p.open);
    assert(!cviii_menu_stack_parent_active(&p,&a));
    assert(!cviii_menu_stack_enter_audio(&p,&a,1,1));
    cviii_audio_menu_state_close(&a);
    assert(!a.open && !p.open); /* standalone F6 returns to gameplay */
    assert(cviii_start_toggle(&p,1,1));
    assert(cviii_start_move(&p,1));
    assert(cviii_start_move(&p,1));
    assert(p.selected==2u); /* AUDIO remains selected through child */
    assert(cviii_menu_stack_parent_active(&p,&a));
    assert(!cviii_menu_stack_enter_audio(&p,&a,0,1));
    assert(!cviii_menu_stack_enter_audio(&p,&a,1,0));
    assert(p.selected==2u && !a.open);
    assert(cviii_menu_stack_enter_audio(&p,&a,1,1));
    assert(a.open && p.open && p.selected==2u);
    assert(!cviii_menu_stack_parent_active(&p,&a));
    assert(!cviii_menu_stack_enter_audio(&p,&a,1,1));
    assert(!cviii_audio_menu_state_move(&a,&levels,1));
    assert(a.selected==1u);
    assert(cviii_audio_menu_state_move(&a,&levels,-2));
    assert(levels.music==99u); /* saved preference, not source gain */
    cviii_audio_menu_state_close(&a); /* F6, Esc, B, Start */
    assert(!a.open && p.open && p.selected==2u);
    assert(cviii_menu_stack_parent_active(&p,&a));
    assert(cviii_start_accept(&p)==CVIII_START_AUDIO);
    cviii_start_close(&p);
    assert(!cviii_menu_stack_parent_active(&p,&a));
    assert(!p.open && !a.open && levels.music==99u);
    assert(!cviii_menu_stack_enter_audio(NULL,&a,1,1));
    assert(!cviii_menu_stack_enter_audio(&p,NULL,1,1));
    puts("PASS JP0626 F6 standalone vs nested Audio→Pause; input focus and selection");
    return 0;
}
