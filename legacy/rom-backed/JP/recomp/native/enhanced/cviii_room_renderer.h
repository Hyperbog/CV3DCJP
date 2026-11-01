/* ROM-backed, read-only, software whole-room background renderer.
 * Presentation only: does not read or alter NES hardware state.
 * Coordinates and output buffers use pixels; metatiles are 32x32 pixels. */
#ifndef CVIII_ROOM_RENDERER_H
#define CVIII_ROOM_RENDERER_H
#include "cviii_room_gfx.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CVIII_ROOM_MAX_DIM 4096u

typedef struct {
    const CviiiRoomGfx *graphics;
    const CviiiWholeRoomView *room;
    const CviiiChrIndex *chr;
    /* Four 1 KiB background windows R4..R7; caller owns actual mapping. */
    uint8_t bg_bank_1k[4];
    /* 16 already-converted ARGB palette entries, from the caller's PPU. */
    uint32_t palette_argb[16];
} CviiiRoomRenderSource;

/* Validate all metatile indexes, inputs, and geometry before touching pixels. */
bool cviii_room_render_validate(const CviiiRoomRenderSource *source);

/* Render any camera-sized rectangle of the full original room to dst.
 * Pixels outside the room use palette_argb[0]. Non-success leaves dst intact.
 * width and height are output pixels, stride is pixels per row. */
bool cviii_room_render_view(const CviiiRoomRenderSource *source,
                            unsigned camera_x, unsigned camera_y,
                            uint32_t *dst, unsigned width, unsigned height,
                            size_t stride);

/* Full scene dimensions; returns false instead of truncating. */
bool cviii_room_render_dimensions(const CviiiRoomRenderSource *source,
                                  unsigned *width, unsigned *height);
#endif