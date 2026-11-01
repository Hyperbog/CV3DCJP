/* JP0623: pure original-font pause menu compositor, no SDL/PPU writes. */
#ifndef CVIII_START_UI_0623_H
#define CVIII_START_UI_0623_H
#include "cviii_start_menu_0623.h"
#include "cviii_slot_picker_0625.h"
#include "cviii_chr_index_0598.h"
#include <stdint.h>
int cviii_start_ui_draw(uint32_t *pixels,unsigned w,unsigned h,
                        const CviiiChrIndex *assets,const CviiiStartMenu *menu);
/* Original-CHR slot submenu occupies the same modal panel, not NES PPU. */
int cviii_start_slot_ui_draw(uint32_t *pixels,unsigned w,unsigned h,
                             const CviiiChrIndex *assets,
                             const CviiiSlotPicker *picker);
#endif
