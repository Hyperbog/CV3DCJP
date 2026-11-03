#ifndef CV3DCJP_COLLISION_H
#define CV3DCJP_COLLISION_H
/* ROM-matched Japanese Mapper 24 collision transformations, NOT live hook.
 * Authoritative ROM SHA256:
 * 958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28
 * Preserves 6502 A/X/Y/P and 2 KiB RAM for supported call boundaries.
 * No claim of cycle, IRQ/NMI or gameplay integration parity.
 */
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define COLLISION_ROM_BYTES 393232u
#define COLLISION_FIXED_PRG_BYTES 16384u
#define COLLISION_RAM_BYTES 2048u

typedef struct CollisionState {
    uint8_t a, x, y, sp, p;
    uint8_t ram[COLLISION_RAM_BYTES];
    uint8_t mapped_prg_bank;  /* $8000-$BFFF bank, mirrors RAM[$23] */
    unsigned mapper_writes;
} CollisionState;

typedef enum CollisionExitKind {
    COLLISION_RTS=1, COLLISION_JSR=2, COLLISION_JMP=3,
    COLLISION_QUERY=4, COLLISION_FALLTHROUGH=5,
    COLLISION_UNSUPPORTED=6
} CollisionExitKind;

typedef struct CollisionExit {
    uint16_t next_pc; /* original ROM execution boundary, not a derived world position */
    CollisionExitKind kind;
    unsigned instructions; /* set only for FCDE, zero otherwise */
    uint8_t ok; /* 1: supported bounded translation, 0: failed closed */
    uint8_t carry;
} CollisionExit;

/* For FCDE, fixed_prg points to physical final 16 KiB (ROM file offset 0x3C010).
 * Function is transactional on invalid bank fingerprint. */
CollisionExit Collision_FCDE(CollisionState *state, const uint8_t fixed_prg[COLLISION_FIXED_PRG_BYTES]);
/* All 34 observed post-JSR caller entries have bounded native continuations.
 * Exits at unresolved subroutines, ROM-table reads and outside targets are explicit.
 * The downstream caller graph is NOT yet complete or runtime integrated. */
CollisionExit Collision_Caller(uint16_t call_pc, CollisionState *state);
/* ROM $89EB, $9B5E, $8723 are bounded bank-14 translations.
 * $8723 alone may suspend at JSR $E648 and must be resumed through E648. */
CollisionExit Collision_Dependency(uint16_t entry_pc, CollisionState *state);
/* Bank-aware $E648 and $8723 chain; requires original full .nes ROM bytes.
 * Unsupported reads/ROM identity mismatches return ok=0 without changing state. */
CollisionExit Collision_Banked(uint16_t entry_pc, CollisionState *state, const uint8_t *rom, size_t rom_bytes);
/* $8ABD, $EF91, $86FB and $968A composition. Only bounded offline behavior. */
CollisionExit Collision_External(uint16_t entry_pc, CollisionState *state, const uint8_t *rom, size_t rom_bytes);
#ifdef __cplusplus
}
#endif
#endif
