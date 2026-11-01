/* JP0601: deterministic read-only presentation evidence aligned to JP0600. */
#ifndef CVIII_FRAME_EVIDENCE_0601_H
#define CVIII_FRAME_EVIDENCE_0601_H
#include "cviii_chr_fetch_0600.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CVIII_FRAME_WIDTH 256u
#define CVIII_FRAME_HEIGHT 240u
#define CVIII_FRAME_PIXELS (CVIII_FRAME_WIDTH * CVIII_FRAME_HEIGHT)
#define CVIII_FRAME_LINES_BYTES (CVIII_FRAME_HEIGHT * 5u)

/* The game-owned adapter serializes each CycLine as five bytes:
 * v little-endian, fine_x, ctrl, mask. No struct/padding assumptions here. */
typedef struct {
    FILE *output;
    uint64_t samples;
    uint64_t input_failures;
} CviiiFrameEvidence;

bool cviii_frame_evidence_open(CviiiFrameEvidence *audit, const char *path);
void cviii_frame_evidence_close(CviiiFrameEvidence *audit);
/* Call after cyc_run_frame and before cviii_chr_fetch_frame_end.
 * Only sampled original PPU 9-bit color indices are hashed.
 * Does not modify or render pixels. Returns whether a row was written. */
bool cviii_frame_evidence_sample(CviiiFrameEvidence *audit,
                                 const CviiiChrFetchAudit *fetch,
                                 const uint16_t *frame_index,
                                 const uint8_t *bg_opaque,
                                 const uint8_t line_bytes[CVIII_FRAME_LINES_BYTES]);
#endif
