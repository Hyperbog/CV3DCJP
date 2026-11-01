/* JP0609: private read-only trace of 28 game sprite objects -> PPU OAM. */
#ifndef CVIII_OBJECT_OAM_TRACE_0609_H
#define CVIII_OBJECT_OAM_TRACE_0609_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define CVIII_OBJECTS 28u
#define CVIII_OAM_BYTES 256u
typedef struct {
    FILE *out;
    uint64_t frames, active_objects, visible_sprites, full_oam_frames;
    uint64_t phase_moves, phase_44_moves;
    int prev_phase;
} CviiiObjectOamTrace;
bool cviii_object_oam_trace_open(CviiiObjectOamTrace *trace, const char *path);
void cviii_object_oam_trace_close(CviiiObjectOamTrace *trace);
void cviii_object_oam_trace_frame(CviiiObjectOamTrace *trace, const uint8_t *ram,
                                 const uint8_t *oam);
#endif
