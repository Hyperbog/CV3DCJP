/* JP0648: source-authenticated JP PRG0C DMC command event planner.
 * Pure host-side events only; NEVER writes original guest APU registers.
 * Caller must authenticate 60 owner-ROM bytes before passing table.
 */
#ifndef CVIII_DMC_SOURCE_0648_H
#define CVIII_DMC_SOURCE_0648_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CVIII_DMC_PROGRAMS 15u
#define CVIII_DMC_TABLE_BYTES (4u * CVIII_DMC_PROGRAMS)
#define CVIII_DMC_WRITES 6u
typedef struct {
    uint16_t address;
    uint8_t value;
} CviiiDmcWrite;
typedef struct {
    uint8_t command, index, priority_after;
    bool accepted, counter_increment;
    size_t write_count;
    CviiiDmcWrite writes[CVIII_DMC_WRITES];
} CviiiDmcPlan;
/* Original valid DMC program indices are 0..14 (command $6F..$7D).
 * The $8908 guard increments $06DD for indices >=$0D even if denied.
 * The six accepted writes exactly follow original $8934..$8953.
 * Returns false without writing out for unbounded/unsupported inputs.
 * table must be the authenticated original 60-byte region $A957..$A992.
 */
bool cviii_dmc_plan_0648(const uint8_t *table, size_t table_size,
                         unsigned command, unsigned old_priority,
                         CviiiDmcPlan *out);
#endif
