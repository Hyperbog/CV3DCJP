/* Japanese VRC6 static room metatile tiles/attributes (ROM-private v1).
 * No CPU/PPU/register writes; final graphics depend on live CHR banks and
 * the caller-supplied PPU background palette. */
#ifndef CVIII_ROOM_GFX_H
#define CVIII_ROOM_GFX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cviii_whole_room.h"
#include "cviii_chr_index_0598.h"
#define CVIII_GFX_GROUPS 15u
#define CVIII_GFX_ROOMS 135u

typedef struct {
    uint8_t id, prg16;
    uint16_t count;
    uint32_t tile_rom_offset, palette_rom_offset;
    const uint8_t *patterns; /* count x 16, four rows of four 8x8 IDs */
    const uint8_t *attributes; /* count x one NES 32x32 attr byte */
} CviiiGfxGroup;
typedef struct {
    uint8_t stage, area, room, tileset_group;
    uint32_t layout_rom_offset;
} CviiiGfxRoom;
typedef struct {
    uint8_t pattern, palette, pixel2;
    uint16_t physical_tile; /* 0..8191, independent of PPU address */
} CviiiGfxPixel;
typedef struct {
    uint8_t *allocation;
    size_t size;
    CviiiGfxGroup groups[CVIII_GFX_GROUPS];
    CviiiGfxRoom rooms[CVIII_GFX_ROOMS];
} CviiiRoomGfx;

bool cviii_room_gfx_decode(const uint8_t *data, size_t size,
                          CviiiGfxGroup groups[CVIII_GFX_GROUPS],
                          CviiiGfxRoom rooms[CVIII_GFX_ROOMS]);
bool cviii_room_gfx_open(CviiiRoomGfx *gfx, const char *filename);
void cviii_room_gfx_close(CviiiRoomGfx *gfx);
/* Validates a geometry's stage/area/room identity and original source. */
const CviiiGfxGroup *cviii_room_gfx_group(const CviiiRoomGfx *gfx,
                                         const CviiiWholeRoomView *room);
/* Caller supplies effective, time-specific mode-0 CHR windows R4..R7. */
bool cviii_room_gfx_sample(const CviiiRoomGfx *gfx,
                           const CviiiWholeRoomView *room,
                           const CviiiChrIndex *chr,
                           const uint8_t bg_bank_1k[4],
                           unsigned x, unsigned y, CviiiGfxPixel *out);
/* Caller supplies time-specific converted ARGB PPU background palette (16).
 * Index 0 is the universal background; no hardcoded RGB/NES palette. */
bool cviii_room_gfx_color(const CviiiGfxPixel *pixel,
                          const uint32_t bg_argb[16], uint32_t *color);
#endif