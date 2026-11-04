#ifndef CV3DCJP_COLLISION_DISPATCH_V11_H
#define CV3DCJP_COLLISION_DISPATCH_V11_H
/* Opt-in original-JP collision observer for NESRecomp's cycle dispatcher.
 * Every callback returns zero: original machine code ALWAYS executes.
 * Requires pinned upstream mod_function_hook support and explicit TOML sites.
 */
#include <stdint.h>

typedef struct CollisionDispatchV11Stats {
    uint64_t entry_hits;
    uint64_t shadow_ready;
    uint64_t probe_rejected;
    uint64_t matched_return;
    uint64_t mismatched_return;
    uint64_t abandoned_entry;
} CollisionDispatchV11Stats;

/* Register before cyc_hooks_validate(); requires matching game.toml sites. */
int CollisionDispatchV11_Register(void);
/* Load owner-local original .nes (checks entire-file FNV and fixed bank).
 * Set only in a private, opt-in host. Not a replacement or a ROM distributor.
 */
int CollisionDispatchV11_LoadRom(const char *path);
/* Enable the two read-only hooks after cyc_hooks_validate(). */
int CollisionDispatchV11_Enable(void);
/* Writes the 15-column entry/return CSV into a new owner-local file. */
int CollisionDispatchV11_TraceOpen(const char *path);
void CollisionDispatchV11_TraceClose(void);
void CollisionDispatchV11_GetStats(CollisionDispatchV11Stats *out);
#endif
