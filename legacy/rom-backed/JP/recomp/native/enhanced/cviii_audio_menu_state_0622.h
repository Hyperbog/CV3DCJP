/* JP0622: host-independent modal state for Enhanced audio options.
 * Gameplay remains on the host side of the pause/input boundary. */
#ifndef CVIII_AUDIO_MENU_STATE_0622_H
#define CVIII_AUDIO_MENU_STATE_0622_H
#include "cviii_audio_levels_0618.h"
typedef struct {
    unsigned selected;
    int open;
} CviiiAudioMenuState;

/* A toggle can open only with Enhanced enabled and authenticated assets. */
int cviii_audio_menu_state_toggle(CviiiAudioMenuState *state,
                                   int enhanced, int assets_ready);
void cviii_audio_menu_state_close(CviiiAudioMenuState *state);
/* -1 up, +1 down, -2 left, +2 right.
 * Return 1 iff a volume changed; selection changes do not trigger a save. */
int cviii_audio_menu_state_move(CviiiAudioMenuState *state,
                                CviiiAudioLevels *levels, int direction);
#endif
