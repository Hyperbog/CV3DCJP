#include "cviii_entity_events_5h8f.h"
#include <stddef.h>
#include <string.h>

void cviii_entity_reset_5h8f(CviiiEntityLedger5H8F *s){
    if(s)memset(s,0,sizeof *s);
}
bool cviii_entity_register_producer_5h8f(CviiiEntityLedger5H8F *s,uint16_t pc){
    if(!s||!pc)return false;
    for(unsigned i=0;i<CVIII_ENTITY_PRODUCERS_5H8F;i++)
        if(s->registered_producers[i]==pc)return true;
    for(unsigned i=0;i<CVIII_ENTITY_PRODUCERS_5H8F;i++)
        if(!s->registered_producers[i]){
            s->registered_producers[i]=pc;return true;
        }
    return false;
}
static int producer_ok(const CviiiEntityLedger5H8F *s,uint16_t pc){
    if(!pc)return 0;
    for(unsigned i=0;i<CVIII_ENTITY_PRODUCERS_5H8F;i++)
        if(s->registered_producers[i]==pc)return 1;
    return 0;
}
static int value_ok(const CviiiEntityEvent5H8F *e){
    switch(e->kind){
    case CVIII_ENTITY_TILE:
        return e->value==CVIII_ENTITY_INTACT||e->value==CVIII_ENTITY_DESTROYED;
    case CVIII_ENTITY_ITEM:
        return e->value==CVIII_ENTITY_PRESENT||e->value==CVIII_ENTITY_COLLECTED;
    case CVIII_ENTITY_ENEMY: case CVIII_ENTITY_BOSS:
        return e->value==CVIII_ENTITY_ACTIVE||e->value==CVIII_ENTITY_DEFEATED;
    case CVIII_ENTITY_DOOR:
        return e->value==CVIII_ENTITY_CLOSED||e->value==CVIII_ENTITY_OPEN;
    case CVIII_ENTITY_OBJECT:
        return e->value==CVIII_ENTITY_PRESENT||e->value==CVIII_ENTITY_DESTROYED||
               e->value==CVIII_ENTITY_ACTIVE;
    default:return 0;
    }
}
void cviii_entity_visit_5h8f(CviiiEntityLedger5H8F *s,
                             uint8_t stage,uint8_t area,uint8_t room){
    if(!s)return;
    if(s->active&&s->stage==stage&&s->area==area&&s->room==room)return;
    int stage_changed=s->active&&s->stage!=stage;
    for(unsigned i=0;i<CVIII_ENTITY_CAP_5H8F;i++){
        CviiiEntityRecord5H8F *r=&s->records[i];
        if(r->used&&(r->event.scope==CVIII_ENTITY_VISIT||
           (stage_changed&&r->event.scope==CVIII_ENTITY_STAGE)))
            memset(r,0,sizeof *r);
    }
    s->stage=stage;s->area=area;s->room=room;s->active=1;s->visits++;
}
static int matches(const CviiiEntityRecord5H8F *r,
                   uint8_t stage,uint8_t area,uint8_t room,
                   uint8_t kind,uint32_t id){
    return r->used&&r->event.stage==stage&&r->event.area==area&&
           r->event.room==room&&r->event.kind==kind&&r->event.id==id;
}
bool cviii_entity_emit_5h8f(CviiiEntityLedger5H8F *s,uint64_t frame,
                            const CviiiEntityEvent5H8F *e){
    if(!s||!e)return false;
    if(!s->active||!frame||!producer_ok(s,e->producer_pc)||!value_ok(e)||
       e->scope<CVIII_ENTITY_VISIT||e->scope>CVIII_ENTITY_SESSION||
       e->stage!=s->stage||e->area!=s->area||e->room!=s->room){
        s->rejected++;return false;
    }
    for(unsigned i=0;i<CVIII_ENTITY_CAP_5H8F;i++){
        CviiiEntityRecord5H8F *r=&s->records[i];
        if(matches(r,e->stage,e->area,e->room,e->kind,e->id)){
            if(frame<r->frame){s->rejected++;return false;}
            r->event=*e;r->frame=frame;s->accepted++;return true;
        }
    }
    for(unsigned i=0;i<CVIII_ENTITY_CAP_5H8F;i++){
        CviiiEntityRecord5H8F *r=&s->records[i];
        if(!r->used){r->used=1;r->frame=frame;r->event=*e;
                     s->accepted++;return true;}
    }
    s->rejected++;return false; /* no eviction or fabricated identity */
}
bool cviii_entity_get_5h8f(const CviiiEntityLedger5H8F *s,
                           uint8_t stage,uint8_t area,uint8_t room,
                           uint8_t kind,uint32_t id,uint8_t *value){
    if(!s||!value)return false;
    for(unsigned i=0;i<CVIII_ENTITY_CAP_5H8F;i++)
        if(matches(&s->records[i],stage,area,room,kind,id)){
            *value=s->records[i].event.value;return true;
        }
    return false;
}
unsigned cviii_entity_count_5h8f(const CviiiEntityLedger5H8F *s){
    unsigned n=0;if(!s)return 0;
    for(unsigned i=0;i<CVIII_ENTITY_CAP_5H8F;i++)n+=s->records[i].used;
    return n;
}
