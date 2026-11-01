/* 5H-8F: explicit, game-event-only mutable entity state.
 * Never infer death, collection, or block destruction from OAM visibility. */
#ifndef CVIII_ENTITY_EVENTS_5H8F_H
#define CVIII_ENTITY_EVENTS_5H8F_H
#include <stdbool.h>
#include <stdint.h>
#define CVIII_ENTITY_CAP_5H8F 512u
#define CVIII_ENTITY_PRODUCERS_5H8F 16u

enum CviiiEntityKind5H8F {
    CVIII_ENTITY_TILE=1, CVIII_ENTITY_ENEMY, CVIII_ENTITY_ITEM,
    CVIII_ENTITY_OBJECT, CVIII_ENTITY_DOOR, CVIII_ENTITY_BOSS
};
enum CviiiEntityValue5H8F {
    CVIII_ENTITY_INTACT=1, CVIII_ENTITY_DESTROYED, CVIII_ENTITY_PRESENT,
    CVIII_ENTITY_COLLECTED, CVIII_ENTITY_ACTIVE, CVIII_ENTITY_DEFEATED,
    CVIII_ENTITY_CLOSED, CVIII_ENTITY_OPEN
};
enum CviiiEntityScope5H8F {
    CVIII_ENTITY_VISIT=1, CVIII_ENTITY_STAGE, CVIII_ENTITY_SESSION
};
typedef struct {
    uint8_t stage,area,room,kind,value,scope;
    uint32_t id; /* Game-authenticated identity; NEVER raw OAM slot. */
    uint16_t producer_pc; /* Verified originating 6502 routine, 0=unverified. */
} CviiiEntityEvent5H8F;
typedef struct {
    CviiiEntityEvent5H8F event;
    uint64_t frame;
    uint8_t used;
} CviiiEntityRecord5H8F;
typedef struct {
    CviiiEntityRecord5H8F records[CVIII_ENTITY_CAP_5H8F];
    uint16_t registered_producers[CVIII_ENTITY_PRODUCERS_5H8F];
    uint8_t stage,area,room,active;
    uint64_t last_frame;
    uint32_t accepted,rejected,visits;
} CviiiEntityLedger5H8F;
void cviii_entity_reset_5h8f(CviiiEntityLedger5H8F *s);
/* Register only after verifying the originating 6502 game routine.
 * No producers are registered by default. */
bool cviii_entity_register_producer_5h8f(CviiiEntityLedger5H8F *s,uint16_t pc);
/* A room switch expires VISIT events; a stage switch also expires STAGE.
 * SESSION entries survive both until power-on/save-state boundary. */
void cviii_entity_visit_5h8f(CviiiEntityLedger5H8F *s,
                             uint8_t stage,uint8_t area,uint8_t room);
/* Only a CPU/game-hook with a verified instruction origin may emit an event.
 * This API does not identify game routines or infer anything from images. */
bool cviii_entity_emit_5h8f(CviiiEntityLedger5H8F *s,uint64_t frame,
                            const CviiiEntityEvent5H8F *event);
bool cviii_entity_get_5h8f(const CviiiEntityLedger5H8F *s,
                           uint8_t stage,uint8_t area,uint8_t room,
                           uint8_t kind,uint32_t id,uint8_t *value);
unsigned cviii_entity_count_5h8f(const CviiiEntityLedger5H8F *s);
/* Runtime entrypoint for verified game-specific CPU hooks; initially unused
 * until the actual routines/entity identifiers are established. */
bool cviii_room_diag_game_entity_event_5h8f(const CviiiEntityEvent5H8F *event);
#endif
