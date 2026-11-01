#ifndef CVIII_PC_WORLD_5H8H_H
#define CVIII_PC_WORLD_5H8H_H
/* PC-owned world model. No NES headers, ROM, SDL, or emulated bus. C11. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CVIII_PC_MAX_ROOMS 160u
#define CVIII_PC_MAX_ENTITIES 1024u
#define CVIII_PC_LAYER_COUNT 3u
#define CVIII_PC_TILE_UNSET UINT16_MAX
#define CVIII_PC_TILE_PX 8

typedef enum { CVIII_PC_TERRAIN=0, CVIII_PC_DECOR=1, CVIII_PC_FRONT=2 } CviiiPcLayer;
typedef enum { CVIII_PC_VISIT=1, CVIII_PC_STAGE=2, CVIII_PC_SESSION=3 } CviiiPcScope;
typedef struct {
    uint16_t base[CVIII_PC_LAYER_COUNT];
    uint16_t changed[CVIII_PC_LAYER_COUNT];
    uint8_t scope[CVIII_PC_LAYER_COUNT];
} CviiiPcTile;
typedef struct {
    uint8_t stage, area, room;
    uint16_t width, height; /* 8x8-cell dimensions; not NES screen counts */
    bool placed;
    int64_t world_x, world_y; /* pixel coordinates; may be negative */
    CviiiPcTile *cells;
} CviiiPcRoom;
typedef struct {
    uint64_t id;           /* room identity + source spawn-slot, never OAM */
    uint8_t stage, area, room, kind, initial_state, state, scope;
    int32_t local_x, local_y; /* entity origin in room-local pixels */
} CviiiPcEntity;
typedef struct {
    CviiiPcRoom rooms[CVIII_PC_MAX_ROOMS];
    CviiiPcEntity entities[CVIII_PC_MAX_ENTITIES];
    size_t room_count, entity_count;
    uint64_t resource_signature; /* supplied by later owner-local importer */
    uint8_t visit_stage, visit_area, visit_room;
    bool has_visit;
} CviiiPcWorld;

CviiiPcWorld *cviii_pc_new(uint64_t resource_signature);
void cviii_pc_free(CviiiPcWorld *w);
CviiiPcRoom *cviii_pc_room(CviiiPcWorld *w, uint8_t stage, uint8_t area, uint8_t room);
const CviiiPcRoom *cviii_pc_room_const(const CviiiPcWorld *w, uint8_t stage, uint8_t area, uint8_t room);
bool cviii_pc_add_room(CviiiPcWorld *w, uint8_t stage, uint8_t area,
                       uint8_t room, uint16_t width, uint16_t height);
bool cviii_pc_place_room(CviiiPcWorld *w, uint8_t stage, uint8_t area,
                         uint8_t room, int64_t world_x, int64_t world_y);
/* Does not guess placement: unknown origins return false. */
bool cviii_pc_world_to_cell(const CviiiPcRoom *r, int64_t world_x,
                            int64_t world_y, uint16_t *cx, uint16_t *cy);
/* Ambiguous overlaps are rejected, never arbitrarily assigned. */
const CviiiPcRoom *cviii_pc_room_at(const CviiiPcWorld *w, uint8_t stage,
                                     int64_t world_x, int64_t world_y);
bool cviii_pc_cell_to_world(const CviiiPcRoom *r, uint16_t cx, uint16_t cy,
                            int64_t *world_x, int64_t *world_y);
bool cviii_pc_set_base(CviiiPcRoom *r, CviiiPcLayer layer,
                       uint16_t x, uint16_t y, uint16_t tile);
bool cviii_pc_get_tile(const CviiiPcRoom *r, CviiiPcLayer layer,
                       uint16_t x, uint16_t y, uint16_t *tile);
/* Explicit native/game-event updates. No OAM/visual heuristic. */
bool cviii_pc_change_tile(CviiiPcRoom *r, CviiiPcLayer layer,
                          uint16_t x, uint16_t y, uint16_t tile, CviiiPcScope scope);
uint64_t cviii_pc_entity_id(uint8_t stage, uint8_t area, uint8_t room, uint16_t spawn_slot);
CviiiPcEntity *cviii_pc_entity(CviiiPcWorld *w, uint64_t id);
bool cviii_pc_add_entity(CviiiPcWorld *w, uint8_t stage, uint8_t area,
                         uint8_t room, uint16_t spawn_slot, uint8_t kind,
                         int32_t local_x, int32_t local_y, uint8_t initial_state);
bool cviii_pc_entity_world_position(const CviiiPcWorld *w, uint64_t id,
                                    int64_t *world_x, int64_t *world_y);
bool cviii_pc_entity_state(CviiiPcWorld *w, uint64_t id,
                           uint8_t new_state, CviiiPcScope scope);
/* Expires VISIT state when changing room; STAGE state when changing stage.
 * SESSION changes survive both until explicit reset/new game. */
void cviii_pc_enter_room(CviiiPcWorld *w, uint8_t stage, uint8_t area, uint8_t room);
/* Save only confirmed stage/session changes, not unverified observations. */
bool cviii_pc_save(const CviiiPcWorld *w, const char *filename);
bool cviii_pc_load(CviiiPcWorld *w, const char *filename);
#endif
