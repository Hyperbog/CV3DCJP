/* JP0622: input-independent modal audio settings control, no emulated RAM. */
#include "cviii_audio_menu_state_0622.h"

int cviii_audio_menu_state_toggle(CviiiAudioMenuState *state,
                                   int enhanced, int assets_ready)
{
    if (!state) return 0;
    if (state->open) {
        state->open=0;
        return 1;
    }
    if (!enhanced || !assets_ready) return 0;
    state->selected=0u;
    state->open=1;
    return 1;
}
void cviii_audio_menu_state_close(CviiiAudioMenuState *state)
{
    if (state) state->open=0;
}
int cviii_audio_menu_state_move(CviiiAudioMenuState *state,
                                CviiiAudioLevels *levels, int direction)
{
    if (!state || !levels || !state->open || state->selected>2u) return 0;
    if (direction==-1) {
        state->selected=(state->selected+2u)%3u;
        return 0;
    }
    if (direction==1) {
        state->selected=(state->selected+1u)%3u;
        return 0;
    }
    if (direction!=2 && direction!=-2) return 0;
    const CviiiVolumeChannel channel=(CviiiVolumeChannel)state->selected;
    const int before=cviii_audio_levels_get(levels,channel);
    if (before<0 ||
        !cviii_audio_levels_step(levels,channel,direction==2?1:-1)) return 0;
    return cviii_audio_levels_get(levels,channel)!=before;
}
