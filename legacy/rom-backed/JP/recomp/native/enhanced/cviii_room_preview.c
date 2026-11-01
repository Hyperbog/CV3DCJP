#include "cviii_room_preview.h"
#include <string.h>

bool cviii_room_preview_compose(const CviiiRoomRenderSource *room,
                                const uint32_t *original_256x240,
                                unsigned camera_x, unsigned camera_y,
                                uint32_t *output_512x240)
{
    if (!original_256x240 || !output_512x240 ||
        !cviii_room_render_validate(room)) return false;
    /* The renderer writes columns 256..511 only. No shared native frame or
     * PPU RAM is touched, and the original 256x240 pixels remain identical. */
    if (!cviii_room_render_view(room, camera_x, camera_y,
                               output_512x240 + 256u, 256u, 240u,
                               CVIII_ROOM_PREVIEW_W)) return false;
    for (unsigned y=0; y<CVIII_ROOM_PREVIEW_H; ++y)
        memcpy(output_512x240+y*CVIII_ROOM_PREVIEW_W,
               original_256x240+y*256u, 256u*sizeof(uint32_t));
    return true;
}