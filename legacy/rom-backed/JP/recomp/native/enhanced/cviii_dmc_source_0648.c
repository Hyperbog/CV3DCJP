/* JP0648 original high-command DMC path, original CPU write schedule.
 * No DPCM decode, playback, APU changes, priority overrides or ROM assets.
 */
#include "cviii_dmc_source_0648.h"
#include <string.h>
bool cviii_dmc_plan_0648(const uint8_t *table,size_t table_size,
                         unsigned command,unsigned old_priority,
                         CviiiDmcPlan *out)
{
    if(!table || !out || table_size!=CVIII_DMC_TABLE_BYTES ||
       command<0x6Fu || command>0x7Du || old_priority>255u)
        return false;
    const unsigned index=command-0x6Fu;
    const bool counter=(index>=0x0Du);
    const bool denied=(old_priority>=5u && index<old_priority);
    CviiiDmcPlan plan;
    memset(&plan,0,sizeof(plan));
    plan.command=(uint8_t)command;
    plan.index=(uint8_t)index;
    plan.counter_increment=counter;
    plan.priority_after=denied ? (uint8_t)old_priority : (uint8_t)index;
    plan.accepted=!denied;
    if(!denied) {
        const size_t k=4u*index;
        plan.write_count=CVIII_DMC_WRITES;
        plan.writes[0]=(CviiiDmcWrite){0x4015u,0x0Fu};
        plan.writes[1]=(CviiiDmcWrite){0x4010u,table[k]};
        plan.writes[2]=(CviiiDmcWrite){0x4011u,table[k+1u]};
        plan.writes[3]=(CviiiDmcWrite){0x4012u,table[k+2u]};
        plan.writes[4]=(CviiiDmcWrite){0x4013u,table[k+3u]};
        plan.writes[5]=(CviiiDmcWrite){0x4015u,0x1Fu};
    }
    *out=plan;
    return true;
}
