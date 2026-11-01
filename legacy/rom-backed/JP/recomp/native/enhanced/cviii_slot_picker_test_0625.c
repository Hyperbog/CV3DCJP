/* JP0625 strict ROM-free slot selector and save/load action boundaries. */
#include "cviii_slot_picker_0625.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    CviiiSlotPicker picker={0};
    unsigned result=77u;
    assert(!cviii_slot_active(&picker));
    assert(!cviii_slot_begin(0,CVIII_START_SAVE,0));
    assert(!cviii_slot_begin(&picker,CVIII_START_AUDIO,0));
    assert(!cviii_slot_begin(&picker,CVIII_START_SAVE,10));
    assert(!cviii_slot_step(&picker,1));
    assert(cviii_slot_accept(&picker,&result)==CVIII_START_NONE && result==77u);
    assert(cviii_slot_begin(&picker,CVIII_START_SAVE,0));
    assert(cviii_slot_active(&picker) && picker.slot==0);
    assert(!cviii_slot_begin(&picker,CVIII_START_LOAD,0));
    assert(!cviii_slot_step(&picker,0));
    assert(cviii_slot_step(&picker,-1) && picker.slot==9u);
    assert(cviii_slot_step(&picker,1) && picker.slot==0u);
    assert(cviii_slot_step(&picker,1) && picker.slot==1u);
    assert(cviii_slot_accept(&picker,NULL)==CVIII_START_NONE);
    assert(cviii_slot_accept(&picker,&result)==CVIII_START_SAVE && result==1u);
    assert(!cviii_slot_active(&picker));
    assert(cviii_slot_accept(&picker,&result)==CVIII_START_NONE && result==1u);
    assert(cviii_slot_begin(&picker,CVIII_START_LOAD,9u));
    assert(cviii_slot_accept(&picker,&result)==CVIII_START_LOAD && result==9u);
    assert(cviii_slot_begin(&picker,CVIII_START_SAVE,3u));
    cviii_slot_cancel(&picker);
    assert(!cviii_slot_active(&picker));
    assert(cviii_slot_begin(&picker,CVIII_START_LOAD,6u));
    cviii_slot_cancel(&picker);
    assert(!cviii_slot_active(&picker));
    cviii_slot_cancel(NULL);
    picker.mode=CVIII_START_QUIT;
    assert(!cviii_slot_active(&picker));
    assert(!cviii_slot_step(&picker,1));
    assert(cviii_slot_accept(&picker,&result)==CVIII_START_NONE);
    puts("PASS JP0625 original-host slot 0..9, wrap, save/load, cancel and safety");
    return 0;
}
