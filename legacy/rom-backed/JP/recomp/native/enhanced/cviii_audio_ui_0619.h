/* JP0619: authentic ROM-CHR health bars and in-game-font renderer; host only. */
#ifndef CVIII_AUDIO_UI_0619_H
#define CVIII_AUDIO_UI_0619_H
#include "cviii_audio_levels_0618.h"
#include "cviii_chr_index_0598.h"
#include <stdbool.h>
#include <stdint.h>
#define CVIII_AUDIO_UI_W 256
#define CVIII_AUDIO_UI_H 240
#define CVIII_AUDIO_UI_FONT_ZERO 0x1041u
#define CVIII_AUDIO_UI_FONT_A 0x1050u
#define CVIII_AUDIO_UI_BAR_EMPTY 0x1085u
#define CVIII_AUDIO_UI_BAR_FULL 0x1093u
/* Returns false and leaves pixels untouched if validated CHR is missing. */
bool cviii_audio_ui_render(uint32_t *pixels, unsigned width, unsigned height,
                           const CviiiChrIndex *assets,
                           const CviiiAudioLevels *volumes, unsigned selected);
/* Interactive modal version; retains original authentic CHR shapes. */
bool cviii_audio_ui_render_mode(uint32_t *pixels,unsigned width,unsigned height,
                                const CviiiChrIndex *assets,
                                const CviiiAudioLevels *volumes,
                                unsigned selected,bool interactive);
#endif
