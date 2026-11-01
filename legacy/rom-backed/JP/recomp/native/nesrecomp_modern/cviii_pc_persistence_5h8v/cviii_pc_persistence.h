#ifndef CVIII_PC_PERSISTENCE_5H8V_H
#define CVIII_PC_PERSISTENCE_5H8V_H
/* Explicit native-world gameplay policy. No ROM, emulator or PPU dependency. */
#include "../cviii_pc_world_5h8h/cviii_pc_world.h"

typedef enum {
    CVIII_PC_PERSISTENT_OBJECT = 1,   /* independently verified durable object */
    CVIII_PC_RESPAWNING_ENEMY = 2,   /* defeat expires at actual room exit */
    CVIII_PC_RESPAWNING_CONTAINER = 3, /* torch / candle: break expires at room exit */
    CVIII_PC_VISIT_DROP = 4          /* verified spawned item: heart, bag, weapon, etc. */
} CviiiPcSpawnPolicy;

typedef struct {
    uint64_t id;
    CviiiPcSpawnPolicy policy;
} CviiiPcSpawnBinding;

typedef struct {
    CviiiPcWorld *world; /* borrowed, must outlive registry */
    CviiiPcSpawnBinding bindings[CVIII_PC_MAX_ENTITIES];
    size_t count;
} CviiiPcLevelPolicy;

void cviii_pc_level_init(CviiiPcLevelPolicy *p, CviiiPcWorld *world);
bool cviii_pc_level_bind(CviiiPcLevelPolicy *p, uint64_t id, CviiiPcSpawnPolicy policy);
/* Never use persistent-object policy for an ordinary candle or torch. */
bool cviii_pc_level_destroy_object(CviiiPcLevelPolicy *p, uint64_t id, uint8_t state);
bool cviii_pc_level_break_container(CviiiPcLevelPolicy *p, uint64_t id, uint8_t broken_state);
bool cviii_pc_level_defeat_enemy(CviiiPcLevelPolicy *p, uint64_t id, uint8_t defeated_state);
/* Actual gameplay event chooses WHAT drops; this API never invents loot. */
bool cviii_pc_level_spawn_drop(CviiiPcLevelPolicy *p, uint64_t container_id,
                               uint64_t drop_id, uint8_t spawned_state);
bool cviii_pc_level_collect_drop(CviiiPcLevelPolicy *p, uint64_t drop_id,
                                 uint8_t collected_state);
/* A verified destroyed tile stays destroyed for this saved run. */
bool cviii_pc_level_destroy_tile(CviiiPcLevelPolicy *p,
    uint8_t stage, uint8_t area, uint8_t room, CviiiPcLayer layer,
    uint16_t x, uint16_t y, uint16_t destroyed_tile);
/* Only call for true room-entry events. A camera move never invokes this. */
bool cviii_pc_level_enter_room(CviiiPcLevelPolicy *p,
    uint8_t stage, uint8_t area, uint8_t room);
#endif
