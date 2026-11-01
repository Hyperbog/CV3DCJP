#include "cviii_pc_persistence.h"
#include <string.h>

void cviii_pc_level_init(CviiiPcLevelPolicy *p, CviiiPcWorld *world) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->world = world;
}
static CviiiPcSpawnPolicy policy_for(const CviiiPcLevelPolicy *p, uint64_t id) {
    if (!p || !p->world) return 0;
    for (size_t i = 0; i < p->count; i++)
        if (p->bindings[i].id == id) return p->bindings[i].policy;
    return 0;
}
static bool valid_policy(CviiiPcSpawnPolicy policy) {
    return policy == CVIII_PC_PERSISTENT_OBJECT ||
           policy == CVIII_PC_RESPAWNING_ENEMY ||
           policy == CVIII_PC_RESPAWNING_CONTAINER ||
           policy == CVIII_PC_VISIT_DROP;
}
bool cviii_pc_level_bind(CviiiPcLevelPolicy *p, uint64_t id, CviiiPcSpawnPolicy policy) {
    if (!p || !p->world || !cviii_pc_entity(p->world, id) || !valid_policy(policy))
        return false;
    CviiiPcSpawnPolicy old = policy_for(p, id);
    if (old) return old == policy; /* Never silently reclassify an object. */
    if (p->count >= CVIII_PC_MAX_ENTITIES) return false;
    p->bindings[p->count++] = (CviiiPcSpawnBinding){id, policy};
    return true;
}
static bool update(CviiiPcLevelPolicy *p, uint64_t id, CviiiPcSpawnPolicy expected,
                   uint8_t state, CviiiPcScope scope) {
    if (policy_for(p, id) != expected) return false;
    CviiiPcEntity *e = cviii_pc_entity(p->world, id);
    if (!e || state == e->initial_state) return false;
    return cviii_pc_entity_state(p->world, id, state, scope);
}
bool cviii_pc_level_destroy_object(CviiiPcLevelPolicy *p, uint64_t id, uint8_t state) {
    return update(p, id, CVIII_PC_PERSISTENT_OBJECT, state, CVIII_PC_SESSION);
}
bool cviii_pc_level_break_container(CviiiPcLevelPolicy *p, uint64_t id, uint8_t state) {
    if (policy_for(p, id) != CVIII_PC_RESPAWNING_CONTAINER) return false;
    CviiiPcEntity *e = cviii_pc_entity(p->world, id);
    if (!e || e->state != e->initial_state) return false; /* no duplicate break */
    return update(p, id, CVIII_PC_RESPAWNING_CONTAINER, state, CVIII_PC_VISIT);
}
bool cviii_pc_level_defeat_enemy(CviiiPcLevelPolicy *p, uint64_t id, uint8_t state) {
    return update(p, id, CVIII_PC_RESPAWNING_ENEMY, state, CVIII_PC_VISIT);
}
bool cviii_pc_level_spawn_drop(CviiiPcLevelPolicy *p, uint64_t source_id,
                               uint64_t drop_id, uint8_t state) {
    if (policy_for(p, source_id) != CVIII_PC_RESPAWNING_CONTAINER ||
        policy_for(p, drop_id) != CVIII_PC_VISIT_DROP || source_id == drop_id)
        return false;
    CviiiPcEntity *source = cviii_pc_entity(p->world, source_id);
    CviiiPcEntity *drop = cviii_pc_entity(p->world, drop_id);
    if (!source || !drop || source->state == source->initial_state ||
        drop->state != drop->initial_state || source->stage != drop->stage ||
        source->area != drop->area || source->room != drop->room) return false;
    return update(p, drop_id, CVIII_PC_VISIT_DROP, state, CVIII_PC_VISIT);
}
bool cviii_pc_level_collect_drop(CviiiPcLevelPolicy *p, uint64_t id, uint8_t state) {
    if (policy_for(p, id) != CVIII_PC_VISIT_DROP) return false;
    CviiiPcEntity *e = cviii_pc_entity(p->world, id);
    if (!e || e->state == e->initial_state) return false;
    return update(p, id, CVIII_PC_VISIT_DROP, state, CVIII_PC_VISIT);
}
bool cviii_pc_level_destroy_tile(CviiiPcLevelPolicy *p,
    uint8_t stage, uint8_t area, uint8_t room, CviiiPcLayer layer,
    uint16_t x, uint16_t y, uint16_t destroyed_tile) {
    if (!p || !p->world) return false;
    CviiiPcRoom *r = cviii_pc_room(p->world, stage, area, room);
    return r && cviii_pc_change_tile(r, layer, x, y, destroyed_tile, CVIII_PC_SESSION);
}
bool cviii_pc_level_enter_room(CviiiPcLevelPolicy *p,
    uint8_t stage, uint8_t area, uint8_t room) {
    if (!p || !p->world || !cviii_pc_room(p->world, stage, area, room)) return false;
    cviii_pc_enter_room(p->world, stage, area, room);
    return true;
}
