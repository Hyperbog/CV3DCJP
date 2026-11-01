/* JP0625: no 6502 state, SDL, filesystem, or save serializer dependency. */
#include "cviii_slot_picker_0625.h"
int cviii_slot_active(const CviiiSlotPicker *picker)
{
    return picker && (picker->mode==CVIII_START_SAVE ||
                      picker->mode==CVIII_START_LOAD) && picker->slot<10u;
}
int cviii_slot_begin(CviiiSlotPicker *picker,CviiiStartAction action,
                     unsigned current_slot)
{
    if(!picker || cviii_slot_active(picker) || current_slot>=10u ||
       (action!=CVIII_START_SAVE && action!=CVIII_START_LOAD))return 0;
    picker->mode=action;
    picker->slot=current_slot;
    return 1;
}
int cviii_slot_step(CviiiSlotPicker *picker,int direction)
{
    if(!cviii_slot_active(picker) || (direction!=1 && direction!=-1))return 0;
    picker->slot=(picker->slot+(direction<0?9u:1u))%10u;
    return 1;
}
void cviii_slot_cancel(CviiiSlotPicker *picker)
{
    if(picker)picker->mode=CVIII_START_NONE;
}
CviiiStartAction cviii_slot_accept(CviiiSlotPicker *picker,unsigned *slot)
{
    if(!slot || !cviii_slot_active(picker))return CVIII_START_NONE;
    *slot=picker->slot;
    const CviiiStartAction action=picker->mode;
    cviii_slot_cancel(picker);
    return action;
}
