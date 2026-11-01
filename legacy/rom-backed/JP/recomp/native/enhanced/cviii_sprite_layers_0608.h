/* JP0608: CPU/PPU-independent PC presentation sprite layers. */
#ifndef CVIII_SPRITE_LAYERS_0608_H
#define CVIII_SPRITE_LAYERS_0608_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CVIII_LAYER_SLOTS 64
#define CVIII_LAYER_H 240
#define CVIII_LAYER_W 256
typedef struct {
    uint8_t valid, x, y, tile, attr, height;
    uint8_t pixels[128]; /* 8x16 color indices; zero is transparent */
} CviiiLayerSprite;
typedef struct {
    CviiiLayerSprite current[CVIII_LAYER_SLOTS];
    CviiiLayerSprite held[CVIII_LAYER_SLOTS];
    unsigned held_now;
    uint8_t phase, prior_phase;
    uint64_t frames, total_held;
    bool primed;
} CviiiSpriteLayers;
typedef struct {
    uint8_t (*read_chr)(void *user, uint16_t address);
    uint32_t (*color)(void *user, unsigned palette_index);
    void *user;
} CviiiLayerFns;
void cviii_layers_reset(CviiiSpriteLayers *layers);
void cviii_layers_capture(CviiiSpriteLayers *layers, const uint8_t oam[256],
                          uint8_t oam_start_phase,
                          const uint8_t sprite_mask[CVIII_LAYER_H],
                          const uint8_t tall[CVIII_LAYER_H],
                          const uint16_t tables[CVIII_LAYER_H],
                          const CviiiLayerFns *functions);
void cviii_layers_draw(const CviiiSpriteLayers *layers, uint32_t *frame,
                       const uint8_t background_opaque[CVIII_LAYER_W * CVIII_LAYER_H],
                       const uint8_t sprite_mask[CVIII_LAYER_H],
                       const CviiiLayerFns *functions);
#endif
