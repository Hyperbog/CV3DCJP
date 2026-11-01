/* JP0614: private audio trace provenance, no NES memory or timing changes. */
#ifndef CVIII_AUDIO_SESSION_0614_H
#define CVIII_AUDIO_SESSION_0614_H
#include <stdint.h>
#include <stdio.h>
typedef struct {
    FILE *output;
    char id[33];
    uint32_t epochs;
    uint64_t prev_requests, prev_writes;
} CviiiAudioSession;
int cviii_audio_session_open(CviiiAudioSession *s, const char *path, const char *id);
void cviii_audio_session_mark(CviiiAudioSession *s, const char *event,
    uint64_t cycle, uint64_t requests, uint64_t writes,
    FILE *request_csv, FILE *register_csv);
void cviii_audio_session_close(CviiiAudioSession *s);
#endif
