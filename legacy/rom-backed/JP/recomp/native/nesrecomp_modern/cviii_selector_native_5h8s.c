/* Phase 5H-8S. Read-only, opt-in selector-write diagnostics.
 * The engine hook runs before the original CPU RAM store; this callback never
 * reads, modifies, or replaces machine RAM, mapper state, or cycle timing. */
#include "cviii_selector_native_5h8s.h"
#include "cviii_room_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static FILE *cviii_trace_file;
static uint64_t cviii_trace_sequence;

static void cviii_selector_witness(uint16_t pc, int32_t prg_offset,
        uint16_t address, uint8_t old_value, uint8_t new_value,
        uint8_t stage, uint8_t area, uint8_t room)
{
    if (!cviii_trace_file) return;
    ++cviii_trace_sequence;
    /* Host frame index is the last completed frame, not the startFrame ID of
       Mesen2. Correlate by selector changes, not by equal frame numbers. */
    unsigned long long completed_frame =
        (unsigned long long)cviii_room_diag_frame_id();
    int32_t bank = prg_offset >= 0 ? prg_offset / 8192 : -1;
    fprintf(cviii_trace_file,
        "%llu,%llu,%u,%d,%d,%u,%u,%u,%u,%u,%u\n",
        completed_frame, (unsigned long long)cviii_trace_sequence,
        (unsigned)pc, (int)bank, (int)prg_offset, (unsigned)address,
        (unsigned)old_value, (unsigned)new_value,
        (unsigned)stage, (unsigned)area, (unsigned)room);
    fflush(cviii_trace_file);
}

void cviii_selector_trace_power_on_5h8s(void)
{
    const char *path = getenv("CVIII_SELECTOR_TRACE_5H8S");
    cyc_cviii_selector_write_5h8s = NULL;
    if (!path || !*path) return;
    if (!cviii_trace_file) {
        cviii_trace_file = fopen(path, "wb");
        if (!cviii_trace_file) {
            fprintf(stderr, "5H8S: cannot open selector trace: %s\n", path);
            return;
        }
        fputs("capture_frame,sequence,pc,prg_bank,prg_offset,address,"
              "old_value,new_value,stage_before,area_before,room_before\n",
              cviii_trace_file);
        fflush(cviii_trace_file);
    }
    cyc_cviii_selector_write_5h8s = cviii_selector_witness;
    fprintf(stderr, "5H8S: native selector writes -> %s\n", path);
}
