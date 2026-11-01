/* JP0610: owner's private read-only 2A03 + VRC6 register-write audit.
 * This is NOT a music/SFX separator: the original driver may discard an SFX
 * before any hardware write. No PCM or machine state is modified.
 */
#ifndef CVIII_AUDIO_BUS_0610_H
#define CVIII_AUDIO_BUS_0610_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef struct {
    FILE *output;
    uint64_t writes, rapid_rewrites, dropped;
    uint64_t last_cycle[40];
    uint8_t last_value[40], seen[40];
} CviiiAudioBus;
int cviii_audio_bus_open(CviiiAudioBus *bus, const char *path);
void cviii_audio_bus_close(CviiiAudioBus *bus);
/* Called from pinned hw_apu.c and hw_mapper.c only for actual CPU writes. */
void cviii_audio_bus_observe(uint64_t cycle, uint16_t addr, uint8_t value);
#endif
