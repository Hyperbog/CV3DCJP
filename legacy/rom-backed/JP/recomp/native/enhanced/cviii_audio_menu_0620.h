/* JP0620: SDL host's modal shell talks to this game-owned UI controller.
 * No platform types, ROM bytes or emulated CPU state flow through this API. */
#ifndef CVIII_AUDIO_MENU_0620_H
#define CVIII_AUDIO_MENU_0620_H
int cviii_audio_menu_active(void);
int cviii_audio_menu_toggle(void);
void cviii_audio_menu_close(void);
/* -1 up, +1 down, -2 left, +2 right; true only if value changed. */
int cviii_audio_menu_move(int direction);
#endif
