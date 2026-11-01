#include "cviii_room_renderer.h"

bool cviii_room_render_validate(const CviiiRoomRenderSource *src)
{
    if (!src || !src->graphics || !src->room || !src->chr ||
        !src->chr->pixels || !src->room->metatiles ||
        !src->graphics->allocation) return false;
    const CviiiWholeRoomView *r = src->room;
    if (!r->width || !r->height ||
        r->width > CVIII_ROOM_MAX_DIM / 32u ||
        r->height > CVIII_ROOM_MAX_DIM / 32u) return false;
    const CviiiGfxGroup *g = cviii_room_gfx_group(src->graphics, r);
    if (!g || !g->patterns || !g->attributes || !g->count) return false;
    for (unsigned i = 0; i < 4u; ++i)
        if (src->bg_bank_1k[i] >= 128u) return false;
    for (size_t i = 0; i < (size_t)r->width * r->height; ++i)
        if (r->metatiles[i] >= g->count) return false;
    return true;
}

bool cviii_room_render_dimensions(const CviiiRoomRenderSource *src,
                                  unsigned *width, unsigned *height)
{
    if (!width || !height || !cviii_room_render_validate(src)) return false;
    *width = (unsigned)src->room->width * 32u;
    *height = (unsigned)src->room->height * 32u;
    return true;
}

bool cviii_room_render_view(const CviiiRoomRenderSource *src,
                            unsigned camera_x, unsigned camera_y,
                            uint32_t *dst, unsigned width, unsigned height,
                            size_t stride)
{
    if (!dst || !width || !height || width > CVIII_ROOM_MAX_DIM ||
        height > CVIII_ROOM_MAX_DIM || stride < width ||
        stride > SIZE_MAX / height || !cviii_room_render_validate(src))
        return false;
    const CviiiWholeRoomView *room = src->room;
    const CviiiGfxGroup *g = cviii_room_gfx_group(src->graphics, room);
    unsigned world_width = (unsigned)room->width * 32u;
    unsigned world_height = (unsigned)room->height * 32u;
    const uint8_t *pixels = src->chr->pixels;
    for (unsigned y = 0; y < height; ++y) {
        /* Use wider coordinates to avoid wrapping camera + viewport. */
        uint64_t world_y = (uint64_t)camera_y + y;
        uint32_t *row = dst + (size_t)y * stride;
        for (unsigned x = 0; x < width; ++x) {
            uint64_t world_x = (uint64_t)camera_x + x;
            if (world_x >= world_width || world_y >= world_height) {
                row[x] = src->palette_argb[0];
                continue;
            }
            unsigned px = (unsigned)world_x, py = (unsigned)world_y;
            unsigned mx = px >> 5, my = py >> 5;
            unsigned id = room->metatiles[(size_t)my * room->width + mx];
            unsigned tx = (px >> 3) & 3u, ty = (py >> 3) & 3u;
            unsigned pattern = g->patterns[id * 16u + ty * 4u + tx];
            unsigned attr = g->attributes[id];
            unsigned quad = (ty / 2u) * 2u + tx / 2u;
            unsigned pal = (attr >> (quad * 2u)) & 3u;
            unsigned bank = src->bg_bank_1k[pattern >> 6];
            unsigned tile = bank * 64u + (pattern & 63u);
            unsigned pixel = pixels[tile * 64u + (py & 7u) * 8u + (px & 7u)];
            if (pixel > 3u) return false; /* Pack/CHR validation guarantees 0..3. */
            row[x] = src->palette_argb[pixel ? pal * 4u + pixel : 0u];
        }
    }
    return true;
}