/* Phase 5H-7: read-only, pixel-qualified Stage 1 camera-page tracker. */
#ifndef CVIII_CAMERA_TRACK_5H7_H
#define CVIII_CAMERA_TRACK_5H7_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint64_t frame;
    int last_scroll;
    int32_t predicted;
    int32_t verified;
    unsigned stage,area,room;
    bool primed,confirmed;
} CviiiCameraTrack5H7;
void cviii_camera_track_reset_5h7(CviiiCameraTrack5H7 *track);
/* Returns up to three room-local pixels, not proof of correctness.
 * Caller MUST compare against native pixels before painting anything. */
unsigned cviii_camera_track_propose_5h7(CviiiCameraTrack5H7 *track,
    uint64_t frame,unsigned stage,unsigned area,unsigned room,
    int scroll,unsigned room_pixels,int32_t out[3]);
/* Reject off-page alternatives after pixel-based calibration. */
bool cviii_camera_track_allowed_5h7(const CviiiCameraTrack5H7 *track,
                                    int32_t candidate);
void cviii_camera_track_confirm_5h7(CviiiCameraTrack5H7 *track,
                                    int32_t pixel);
#endif
