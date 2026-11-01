/* 5G: observable, fail-open 426px presentation; never touches NES machine state. */
#ifndef CVIII_MODERN_ROOM_DIAG_H
#define CVIII_MODERN_ROOM_DIAG_H
#include <stdbool.h>
#include <stdint.h>

enum { CVIII_ROOM_OFF=0, CVIII_ROOM_COMPARE=1, CVIII_ROOM_EXTENDED=2 };
bool cviii_room_diag_mode(int mode);
int cviii_room_diag_get_mode(void);
/* Source policy: auto, room, ppu, or atlas (strict indexed world). */
bool cviii_room_diag_source(const char *name);
const char *cviii_room_diag_get_source(void);
bool cviii_room_diag_load_chr(const char *path);
bool cviii_room_diag_load_rooms(const char *path);
bool cviii_room_diag_load_graphics(const char *path);
/* Private 5H-1 indexed-world binary, generated locally from original packs. */
bool cviii_room_diag_index(const char *path);
/* Optional owner-local CSV. No ROM data, only read-only frame observations. */
bool cviii_room_diag_trace(const char *path);
void cviii_room_diag_boundary(void);
void cviii_room_diag_frame_end(void);
uint64_t cviii_room_diag_frame_id(void);
/* Mode 2 always returns a 426x240 picture, even without room packs.
 * Original edition bypasses this function at the host level. */
const uint32_t *cviii_room_diag_present(const uint32_t *original,
                                       int *width, int *height);
void cviii_room_diag_center_copy(uint32_t *dst, unsigned width, unsigned height,
                                 const uint32_t *original, uint32_t matte);
#endif
