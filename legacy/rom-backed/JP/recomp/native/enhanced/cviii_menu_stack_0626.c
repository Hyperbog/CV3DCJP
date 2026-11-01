/* JP0626: parent remains resident during child audio page; focus transfers. */
#include "cviii_menu_stack_0626.h"

int cviii_menu_stack_parent_active(const CviiiStartMenu *parent,
                                   const CviiiAudioMenuState *audio)
{
    return parent && parent->open && (!audio || !audio->open);
}
int cviii_menu_stack_enter_audio(const CviiiStartMenu *parent,
                                 CviiiAudioMenuState *audio,
                                 int enhanced,int authentic_chr)
{
    if(!cviii_menu_stack_parent_active(parent,audio) || !audio)
        return 0;
    return cviii_audio_menu_state_toggle(audio,enhanced,authentic_chr);
}
