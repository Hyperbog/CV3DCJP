/* JP0646: read-only native counterpart to JP0617 Mesen sound decisions.
 * No RAM writes, sound classification, virtual voices, or CPU stepping. */
#include "cviii_sound_decisions_0646.h"
#include <inttypes.h>
#include <string.h>
static CviiiSoundDecisions *active;
enum { ROW_LIMIT=100000 };
static void event(CviiiSoundDecisions *t,const char *kind,uint64_t cycle,
                  uint16_t pc,const uint8_t ram[2048])
{
    if(t->rows>=ROW_LIMIT){++t->dropped;return;}
    const uint32_t id=t->pending?t->pending_id:t->next_id;
    const unsigned command=ram?ram[0xEF]:0u;
    const unsigned low=ram?ram[0x109]:0u;
    const unsigned priority=ram?ram[0x19E]:0u;
    const unsigned count=ram?ram[0x6DD]:0u;
    if(fprintf(t->output,"%s,%" PRIu32 ",%" PRIu64 ",%04X,%02X,%02X,%02X,%02X\n",
               kind,id,cycle,(unsigned)pc,command,low,priority,count)<0) {
        ++t->faults;
    }
    ++t->rows;
    if((t->rows&127u)==0u)fflush(t->output);
}
static void fault(CviiiSoundDecisions *t,const char *kind,uint64_t cycle,
                  uint16_t pc,const uint8_t ram[2048])
{
    ++t->faults;
    event(t,kind,cycle,pc,ram);
}
int cviii_sound_decisions_open(CviiiSoundDecisions *t,const char *path)
{
    if(!t||!path||!*path||active)return 0;
    FILE *f=fopen(path,"w");
    if(!f)return 0;
    memset(t,0,sizeof(*t));
    t->output=f;
    active=t;
    if(fputs("event,id,cycle,pc,command,low_state,priority,counter\n",f)<0) {
        cviii_sound_decisions_close(t);
        return 0;
    }
    return 1;
}
void cviii_sound_decisions_close(CviiiSoundDecisions *t)
{
    if(!t)return;
    if(active==t)active=NULL;
    if(t->output)fclose(t->output);
    t->output=NULL;
}
void cviii_sound_decisions_boundary(const char *name,uint64_t cycle)
{
    CviiiSoundDecisions *t=active;
    if(!t||!t->output||!name)return;
    if(strcmp(name,"power_on") && strcmp(name,"state_loaded"))return;
    if(t->pending)fault(t,"invalid_inflight_boundary",cycle,0,NULL);
    event(t,name,cycle,0,NULL);
    t->pending=0;
    t->metadata=0;
    t->seen_cycle=0;
    fflush(t->output);
}
static const char *terminal(uint8_t command,uint8_t low,uint8_t priority)
{
    if(command<0x50u)
        return low==0x50u && command>=6u?"low_return":"low_install";
    if(command<0x6Fu)return "meta_install";
    const unsigned index=(unsigned)command-0x6Fu;
    return priority>=5u && index<priority?"dmc_return":"dmc_program";
}
void cviii_sound_decisions_fetch(uint64_t cycle,uint16_t pc,uint8_t opcode,
                                 const uint8_t ram[2048])
{
    CviiiSoundDecisions *t=active;
    if(!t||!t->output||!ram)return;
    const char *kind=NULL;
    uint8_t witness=0;
    switch(pc) {
    case 0x8852u:kind="entry";witness=0xC9u;break;
    case 0x885Du:kind="meta";witness=0x20u;break;
    case 0x884Fu:kind="low_return";witness=0x60u;break;
    case 0x8860u:kind="low_install";witness=0x86u;break;
    case 0x8922u:kind="dmc_return";witness=0x60u;break;
    case 0x8928u:kind="dmc_program";witness=0x84u;break;
    default:return;
    }
    if(t->seen_cycle && cycle<t->last_cycle)
        fault(t,"invalid_cycle_regression",cycle,pc,ram);
    t->seen_cycle=1;
    t->last_cycle=cycle;
    if(opcode!=witness){
        fault(t,"invalid_opcode",cycle,pc,ram);
        return;
    }
    if(pc==0x8852u){
        if(t->pending)
            fault(t,"invalid_nested",cycle,pc,ram);
        t->pending=1;
        t->pending_id=t->next_id++;
        t->metadata=0;
        t->command=ram[0xEF];
        t->low=ram[0x109];
        t->priority=ram[0x19E];
        t->counter=ram[0x6DD];
        event(t,kind,cycle,pc,ram);
        return;
    }
    if(!t->pending){
        fault(t,"invalid_orphan",cycle,pc,ram);
        return;
    }
    if(pc==0x885Du){
        if(t->metadata || t->command<0x50u || t->command>=0x6Fu)
            fault(t,"invalid_metadata",cycle,pc,ram);
        t->metadata=1;
        event(t,"meta",cycle,pc,ram);
        return;
    }
    const char *expected=terminal(t->command,t->low,t->priority);
    if(pc==0x8860u && t->metadata)
        kind="meta_install";
    if(strcmp(kind,expected) || (t->command>=0x50u &&
         t->command<0x6Fu && !t->metadata))
        fault(t,"invalid_decision",cycle,pc,ram);
    event(t,kind,cycle,pc,ram);
    t->pending=0;
    t->metadata=0;
}
