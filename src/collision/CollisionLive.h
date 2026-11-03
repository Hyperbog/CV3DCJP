#ifndef CV3DCJP_COLLISION_LIVE_H
#define CV3DCJP_COLLISION_LIVE_H
/* Opt-in, read-only CPU substitution PROBE. Does not change the host CPU.
 * Full substitution requires externally verified CPU/PPU/APU/mapper bus timing.
 */
#include "Collision.h"
#include <stdint.h>
#include <stddef.h>

typedef struct CollisionLiveSnapshot {
    CollisionState cpu;
    uint16_t pc;                    /* Must be $FCDE, AFTER caller JSR */
    uint64_t cpu_cycles;            /* At fetch of $FCDE, not at caller JSR */
    uint8_t interrupt_pending;
    uint8_t dma_active;
    uint8_t mapper_irq_pending;
} CollisionLiveSnapshot;

typedef enum CollisionLiveStatus {
    COLLISION_LIVE_READY_FOR_SHADOW = 0,
    COLLISION_LIVE_INVALID_ARGUMENT = 1,
    COLLISION_LIVE_WRONG_ENTRY = 2,
    COLLISION_LIVE_WRONG_BANK = 3,
    COLLISION_LIVE_UNVERIFIED_CALLER = 4,
    COLLISION_LIVE_STACK_MISMATCH = 5,
    COLLISION_LIVE_ASYNC_HARDWARE = 6,
    COLLISION_LIVE_NATIVE_REJECTED = 7
} CollisionLiveStatus;

typedef struct CollisionLivePreview {
    CollisionLiveSnapshot after;
    CollisionTimedExit query;       /* Includes native $FCDE..RTS cycles */
    uint16_t caller_pc;
    CollisionLiveStatus status;
    uint8_t runtime_commit_authorized; /* Always zero until bus parity gate */
} CollisionLivePreview;

/* Provide original, authenticated .nes contents (not a generated asset pack).
 * Refuses nonbank14 callers, non-$FCDE entry, mismatched JSR bytes/stack,
 * async hardware and ROM fingerprint errors. Source snapshot never modified.
 * This is a PREVIEW, not a live CPU/PPU clock advancement or game hook.
 */
CollisionLiveStatus CollisionLive_ProbeFCDE(
    const CollisionLiveSnapshot *before,
    uint16_t caller_pc,
    const uint8_t *rom, size_t rom_bytes,
    CollisionLivePreview *preview);
#endif
