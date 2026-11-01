/* JP0646: owner-only original sound-dispatch boundary observation.
 * Never changes emulated CPU/mapper/APU hardware or audio samples. */
#ifndef CVIII_SOUND_DECISIONS_0646_H
#define CVIII_SOUND_DECISIONS_0646_H
#include <stdint.h>
#include <stdio.h>
typedef struct {
    FILE *output;
    uint64_t rows, dropped, faults, last_cycle;
    uint32_t next_id, pending_id;
    uint8_t command, low, priority, counter, metadata;
    int pending, seen_cycle;
} CviiiSoundDecisions;
int cviii_sound_decisions_open(CviiiSoundDecisions *trace,const char *path);
void cviii_sound_decisions_close(CviiiSoundDecisions *trace);
void cviii_sound_decisions_boundary(const char *name,uint64_t cycle);
/* Call only on a real, non-discarded mapper24 opcode fetch. The caller
 * verifies tracked bank $0C and fixed wrapper $26==$FF separately.
 * ram is a read-only pointer to at least 2048 bytes of CPU RAM. */
void cviii_sound_decisions_fetch(uint64_t cycle,uint16_t pc,uint8_t opcode,
                                  const uint8_t ram[2048]);
#endif
