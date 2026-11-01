#ifndef CVIII_ROOM_NATIVE_HOST_0660_H
#define CVIII_ROOM_NATIVE_HOST_0660_H
#include "cviii_room_native_bridge_0660.h"
#include <stdint.h>
/* Read-only counters, never part of emulated machine state. */
typedef struct {
    uint64_t operand_reads, site_reads;
    uint64_t wrong_bank, pending_interrupt, wrong_context, wrong_opcode;
    uint64_t rts_while_active, unmatched_rts;
} CviiiRoomNativeHostDiagnostics0660;
void cviii_bridge0660_diagnostics(CviiiRoomNativeHostDiagnostics0660 *out);
/* Attach only when all required NESRecomp observers are free; no chaining
 * across unknown plugins. Original CPU always executes unchanged. */
int cviii_bridge0660_attach(CviiiRoomNativeBridge0660 *bridge);
void cviii_bridge0660_boundary(CviiiRoomNativeBridge0660 *bridge);
#endif