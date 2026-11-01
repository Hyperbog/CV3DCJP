/* Opt-in 512x240 side-by-side engineering preview. The left frame remains
 * the native NES output; the right frame uses immutable ROM-derived assets. */
#ifndef CVIII_ROOM_PREVIEW_HEADER_H
#define CVIII_ROOM_PREVIEW_HEADER_H
#include "cviii_room_renderer.h"
#include <stdbool.h>
#include <stdint.h>
#define CVIII_ROOM_PREVIEW_W 512u
#define CVIII_ROOM_PREVIEW_H 240u
bool cviii_room_preview_compose(const CviiiRoomRenderSource *room,
                                const uint32_t *original_256x240,
                                unsigned camera_x, unsigned camera_y,
                                uint32_t *output_512x240);
#endif