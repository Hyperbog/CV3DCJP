#include "cviii_entity_events_5h8f.h"
#include <assert.h>
#include <stdio.h>
static CviiiEntityLedger5H8F ledger;
static CviiiEntityEvent5H8F e(uint8_t kind,uint8_t value,
                              uint8_t scope,uint32_t id){
    CviiiEntityEvent5H8F r={0,1,0,kind,value,scope,id,0xE04Du};return r;
}
int main(void){
    cviii_entity_visit_5h8f(&ledger,0,1,0);
    CviiiEntityEvent5H8F tile=e(CVIII_ENTITY_TILE,CVIII_ENTITY_DESTROYED,
                               CVIII_ENTITY_VISIT,0x000C0014u);
    CviiiEntityEvent5H8F item=e(CVIII_ENTITY_ITEM,CVIII_ENTITY_COLLECTED,
                               CVIII_ENTITY_STAGE,42);
    CviiiEntityEvent5H8F boss=e(CVIII_ENTITY_BOSS,CVIII_ENTITY_DEFEATED,
                               CVIII_ENTITY_SESSION,7);
    uint8_t value=0;
    /* No event can be accepted before the originating game routine is whitelisted. */
    assert(!cviii_entity_emit_5h8f(&ledger,10,&tile));
    assert(cviii_entity_register_producer_5h8f(&ledger,0xE04Du));
    assert(cviii_entity_emit_5h8f(&ledger,10,&tile));
    assert(cviii_entity_emit_5h8f(&ledger,11,&item));
    assert(cviii_entity_emit_5h8f(&ledger,12,&boss));
    assert(cviii_entity_count_5h8f(&ledger)==3);
    assert(cviii_entity_get_5h8f(&ledger,0,1,0,CVIII_ENTITY_TILE,
                                 tile.id,&value)&&value==CVIII_ENTITY_DESTROYED);
    CviiiEntityEvent5H8F fake=e(CVIII_ENTITY_ENEMY,CVIII_ENTITY_DEFEATED,
                                CVIII_ENTITY_VISIT,1);
    fake.producer_pc=0;
    assert(!cviii_entity_emit_5h8f(&ledger,13,&fake));
    fake.producer_pc=0xE04Du;
    assert(!cviii_entity_emit_5h8f(&ledger,13,&(CviiiEntityEvent5H8F)
       {0,1,0,CVIII_ENTITY_ITEM,CVIII_ENTITY_DEFEATED,CVIII_ENTITY_VISIT,1,0xE04D}));
    assert(!cviii_entity_emit_5h8f(&ledger,9,&tile));
    cviii_entity_visit_5h8f(&ledger,0,1,1);
    assert(!cviii_entity_get_5h8f(&ledger,0,1,0,CVIII_ENTITY_TILE,tile.id,&value));
    assert(cviii_entity_count_5h8f(&ledger)==2);
    cviii_entity_visit_5h8f(&ledger,0,1,0);
    assert(cviii_entity_get_5h8f(&ledger,0,1,0,CVIII_ENTITY_ITEM,item.id,&value));
    cviii_entity_visit_5h8f(&ledger,1,0,0);
    assert(!cviii_entity_get_5h8f(&ledger,0,1,0,CVIII_ENTITY_ITEM,item.id,&value));
    assert(cviii_entity_get_5h8f(&ledger,0,1,0,CVIII_ENTITY_BOSS,boss.id,&value));
    cviii_entity_reset_5h8f(&ledger);
    assert(cviii_entity_count_5h8f(&ledger)==0);
    puts("PASS: producer guard, type validation, visit/stage/session, reset");
    return 0;
}
