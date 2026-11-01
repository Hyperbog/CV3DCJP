/* JP0625: host-independent quick-save/load slot submenu; game-owned state. */
#ifndef CVIII_SLOT_PICKER_0625_H
#define CVIII_SLOT_PICKER_0625_H
#include "cviii_start_menu_0623.h"
typedef struct {
    CviiiStartAction mode; /* NONE, SAVE or LOAD only */
    unsigned slot;        /* 0..9, legacy pinned-host filename convention */
} CviiiSlotPicker;
/* Begin only for a valid core action and selected existing host slot. */
int cviii_slot_begin(CviiiSlotPicker *picker,CviiiStartAction action,
                     unsigned current_slot);
int cviii_slot_active(const CviiiSlotPicker *picker);
int cviii_slot_step(CviiiSlotPicker *picker,int direction);
void cviii_slot_cancel(CviiiSlotPicker *picker);
/* Returns action (SAVE or LOAD) and slot, clears submenu before host I/O. */
CviiiStartAction cviii_slot_accept(CviiiSlotPicker *picker,unsigned *slot);
#endif
