/* JP0612: owner-local read-only original JP $E249 sound-request ingress. */
#ifndef CVIII_SOUND_REQUESTS_0612_H
#define CVIII_SOUND_REQUESTS_0612_H
#include <stdint.h>
#include <stdio.h>
typedef struct {
    FILE *output;
    uint64_t requests, dropped;
} CviiiSoundRequests;
int cviii_sound_requests_open(CviiiSoundRequests *trace, const char *path);
void cviii_sound_requests_close(CviiiSoundRequests *trace);
/* Only call on a real, non-interrupted opcode fetch at fixed $E249.
 * Caller supplies snapshots; this module never reads/writes emulated bus.
 * Stack bytes are untrusted observations, NOT an established caller PC. */
void cviii_sound_requests_observe(uint64_t cycle, uint16_t pc,
    uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p,
    uint8_t tracked_bank, uint8_t stack_lo, uint8_t stack_hi);
#endif
