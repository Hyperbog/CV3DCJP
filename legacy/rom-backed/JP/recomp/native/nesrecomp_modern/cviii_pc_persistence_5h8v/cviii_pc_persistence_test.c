#include "cviii_pc_persistence.h"
#include <stdio.h>
#include <stdint.h>

/* CHECK must remain active in Release (unlike standard assert). */
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; \
} } while (0)
static uint16_t get_tile(const CviiiPcRoom *r, uint16_t x, uint16_t y) {
    uint16_t value = UINT16_MAX;
    if (!cviii_pc_get_tile(r, CVIII_PC_TERRAIN, x, y, &value)) return UINT16_MAX;
    return value;
}
int main(void) {
    const char *save = "cviii-persistent-test.state";
    CviiiPcWorld *w = cviii_pc_new(UINT64_C(0x8ABCDEF017236945));
    CHECK(w);
    CHECK(cviii_pc_add_room(w, 0, 1, 3, 16, 12));
    CHECK(cviii_pc_add_room(w, 0, 1, 4, 16, 12));
    CviiiPcRoom *r = cviii_pc_room(w, 0, 1, 3);
    CHECK(cviii_pc_set_base(r, CVIII_PC_TERRAIN, 2, 3, 91));
    CHECK(cviii_pc_add_entity(w, 0, 1, 3, 11, 1, 16, 24, 0)); /* torch */
    CHECK(cviii_pc_add_entity(w, 0, 1, 3, 12, 1, 24, 24, 0)); /* candle */
    CHECK(cviii_pc_add_entity(w, 0, 1, 3, 13, 2, 32, 24, 0)); /* enemy */
    CHECK(cviii_pc_add_entity(w, 0, 1, 3, 14, 3, 40, 24, 0)); /* durable */
    CHECK(cviii_pc_add_entity(w, 0, 1, 3, 15, 4, 16, 24, 0)); /* heart drop */
    CHECK(cviii_pc_add_entity(w, 0, 1, 3, 16, 4, 24, 24, 0)); /* weapon drop */
    CHECK(cviii_pc_add_entity(w, 0, 1, 3, 17, 4, 16, 24, 0)); /* money bag */
    CHECK(cviii_pc_add_entity(w, 0, 1, 4, 18, 4, 0, 0, 0)); /* cross-room drop */
    const uint64_t torch = cviii_pc_entity_id(0, 1, 3, 11);
    const uint64_t candle = cviii_pc_entity_id(0, 1, 3, 12);
    const uint64_t enemy = cviii_pc_entity_id(0, 1, 3, 13);
    const uint64_t durable = cviii_pc_entity_id(0, 1, 3, 14);
    const uint64_t heart = cviii_pc_entity_id(0, 1, 3, 15);
    const uint64_t weapon = cviii_pc_entity_id(0, 1, 3, 16);
    const uint64_t bag = cviii_pc_entity_id(0, 1, 3, 17);
    const uint64_t remote = cviii_pc_entity_id(0, 1, 4, 18);
    CviiiPcLevelPolicy p;
    cviii_pc_level_init(&p, w);
    CHECK(cviii_pc_level_bind(&p, torch, CVIII_PC_RESPAWNING_CONTAINER));
    CHECK(cviii_pc_level_bind(&p, candle, CVIII_PC_RESPAWNING_CONTAINER));
    CHECK(cviii_pc_level_bind(&p, enemy, CVIII_PC_RESPAWNING_ENEMY));
    CHECK(cviii_pc_level_bind(&p, durable, CVIII_PC_PERSISTENT_OBJECT));
    CHECK(cviii_pc_level_bind(&p, heart, CVIII_PC_VISIT_DROP));
    CHECK(cviii_pc_level_bind(&p, weapon, CVIII_PC_VISIT_DROP));
    CHECK(cviii_pc_level_bind(&p, bag, CVIII_PC_VISIT_DROP));
    CHECK(cviii_pc_level_bind(&p, remote, CVIII_PC_VISIT_DROP));
    CHECK(cviii_pc_level_bind(&p, torch, CVIII_PC_RESPAWNING_CONTAINER));
    CHECK(!cviii_pc_level_bind(&p, torch, CVIII_PC_PERSISTENT_OBJECT));
    CHECK(cviii_pc_level_enter_room(&p, 0, 1, 3));
    CHECK(!cviii_pc_level_spawn_drop(&p, torch, heart, 1)); /* torch intact */
    CHECK(!cviii_pc_level_destroy_object(&p, torch, 1)); /* never permanent */
    CHECK(cviii_pc_level_break_container(&p, torch, 1));
    CHECK(!cviii_pc_level_break_container(&p, torch, 1)); /* no double break */
    CHECK(cviii_pc_level_spawn_drop(&p, torch, heart, 1));
    CHECK(!cviii_pc_level_spawn_drop(&p, torch, heart, 1)); /* no duplicate */
    CHECK(!cviii_pc_level_spawn_drop(&p, torch, remote, 1)); /* room safety */
    CHECK(cviii_pc_level_collect_drop(&p, heart, 2));
    CHECK(cviii_pc_level_break_container(&p, candle, 1));
    CHECK(cviii_pc_level_spawn_drop(&p, candle, weapon, 1));
    CHECK(cviii_pc_level_defeat_enemy(&p, enemy, 1));
    CHECK(cviii_pc_level_destroy_object(&p, durable, 1));
    CHECK(cviii_pc_level_destroy_tile(&p, 0, 1, 3, CVIII_PC_TERRAIN, 2, 3, 0));
    CHECK(!cviii_pc_level_destroy_tile(&p, 0, 1, 3, CVIII_PC_TERRAIN, 16, 3, 0));
    CHECK(cviii_pc_level_enter_room(&p, 0, 1, 3)); /* same room = no reset */
    CHECK(cviii_pc_entity(w, torch)->state == 1);
    CHECK(cviii_pc_entity(w, heart)->state == 2);
    CHECK(get_tile(r, 2, 3) == 0);
    CHECK(cviii_pc_level_enter_room(&p, 0, 1, 4));
    CHECK(cviii_pc_entity(w, torch)->state == 0);  /* respawn */
    CHECK(cviii_pc_entity(w, candle)->state == 0); /* respawn */
    CHECK(cviii_pc_entity(w, enemy)->state == 0);  /* respawn */
    CHECK(cviii_pc_entity(w, heart)->state == 0);  /* transient drop cleared */
    CHECK(cviii_pc_entity(w, weapon)->state == 0);
    CHECK(cviii_pc_entity(w, durable)->state == 1); /* durable */
    CHECK(get_tile(r, 2, 3) == 0); /* wall remains cleared */
    CHECK(cviii_pc_level_enter_room(&p, 0, 1, 3));
    CHECK(cviii_pc_level_break_container(&p, torch, 1));
    CHECK(cviii_pc_level_spawn_drop(&p, torch, bag, 1));
    CHECK(cviii_pc_entity(w, bag)->state == 1);
    CHECK(cviii_pc_save(w, save)); /* VISIT objects and drops omitted */
    CHECK(cviii_pc_entity_state(w, durable, 0, CVIII_PC_SESSION));
    CHECK(cviii_pc_change_tile(r, CVIII_PC_TERRAIN, 2, 3, 91, CVIII_PC_SESSION));
    CHECK(cviii_pc_load(w, save));
    CHECK(cviii_pc_entity(w, durable)->state == 1);
    CHECK(cviii_pc_entity(w, torch)->state == 0);
    CHECK(cviii_pc_entity(w, bag)->state == 0);
    CHECK(get_tile(r, 2, 3) == 0);
    CHECK(remove(save) == 0);
    cviii_pc_free(w);
    puts("PASS: terrain persists; candles, torches, enemies and drops reset; loot is event-selected");
    return 0;
}
