/* State-only evidence. No CPU/PPU writes or speculative scene rendering. */
#ifndef CVIII_SCENE_EPOCH_5H8C_H
#define CVIII_SCENE_EPOCH_5H8C_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { CVIII_EPOCH_ROOM=1, CVIII_EPOCH_MAJOR_CIRAM=2,
       CVIII_EPOCH_CHR=4, CVIII_EPOCH_PALETTE=8,
       CVIII_EPOCH_DISCONTINUITY=16 };
typedef struct {
    uint64_t frame, last_major_frame;
    uint8_t stage, area, room, initialized, palette[32], ciram[2048];
    uint32_t epoch, revision, chr_hash;
} CviiiSceneEpoch5H8C;
typedef struct {
    unsigned flags, ciram_changed;
    uint32_t epoch, revision;
} CviiiSceneEvent5H8C;
void cviii_scene_epoch_reset_5h8c(CviiiSceneEpoch5H8C *s);
bool cviii_scene_epoch_observe_5h8c(CviiiSceneEpoch5H8C *s,
    uint64_t frame,uint8_t stage,uint8_t area,uint8_t room,
    const uint8_t *ciram,size_t ciram_len,
    const uint8_t palette[32],uint32_t mapped_chr_hash,
    CviiiSceneEvent5H8C *out);
#endif
