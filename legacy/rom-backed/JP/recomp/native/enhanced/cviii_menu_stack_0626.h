/* JP0626: game-owned modal layering for Start/Pause -> Audio -> Start/Pause.
 * No SDL, savestate, emulator hardware or original ROM dependencies. */
#ifndef CVIII_MENU_STACK_0626_H
#define CVIII_MENU_STACK_0626_H
#include "cviii_start_menu_0623.h"
#include "cviii_audio_menu_state_0622.h"
int cviii_menu_stack_parent_active(const CviiiStartMenu *parent,
                                   const CviiiAudioMenuState *audio);
int cviii_menu_stack_enter_audio(const CviiiStartMenu *parent,
                                 CviiiAudioMenuState *audio,
                                 int enhanced,int authentic_chr);
#endif
