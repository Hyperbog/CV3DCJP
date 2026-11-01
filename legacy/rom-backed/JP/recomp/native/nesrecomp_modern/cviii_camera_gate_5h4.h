/* Pure evidence-gate helpers; never alter guest hardware or infer unknown edges. */
#ifndef CVIII_CAMERA_GATE_5H4_H
#define CVIII_CAMERA_GATE_5H4_H
#include <stdint.h>
typedef enum {
 CVIII_GATE_NOT_EVALUATED=0, CVIII_GATE_ASSETS_MISSING=1,
 CVIII_GATE_RAM_MISSING=2, CVIII_GATE_ROOM_UNKNOWN=3,
 CVIII_GATE_GRAPHICS_UNKNOWN=4, CVIII_GATE_CHR_UNRESOLVED=5,
 CVIII_GATE_SOURCE_INVALID=6, CVIII_GATE_CAMERA_MISMATCH=7,
 CVIII_GATE_WORLD_UNPLACED=8, CVIII_GATE_NO_MARGIN=9,
 CVIII_GATE_ATLAS_PRESENTED=10, CVIII_GATE_ROOM_PRESENTED=11,
 CVIII_GATE_FORCED_PPU=12, CVIII_GATE_PPU_CHECKED=13,
 CVIII_GATE_PPU_REJECTED=14, CVIII_GATE_LOCAL_PRESENTED=15,
 CVIII_GATE_LOCAL_CAMERA_MISMATCH=16,
 CVIII_GATE_DUAL_PRESENTED=17, CVIII_GATE_DYNAMIC_PRESENTED=18
} CviiiCameraGate5H4;
typedef enum { CVIII_ANCHOR_NONE=0, CVIII_ANCHOR_RAM=1,
               CVIII_ANCHOR_PPU_SCROLL=2, CVIII_ANCHOR_ROOM_ORIGIN=3,
               CVIII_ANCHOR_TRACKED_SCROLL=4, CVIII_ANCHOR_PAGE_PROBE=5 } CviiiCameraAnchor5H4;
typedef struct {unsigned pixel; unsigned source;} CviiiCameraSeed5H4;
/* First verified Stage 1 doorway may be anchored to measured PPU scroll.
 * RAM $56/$57 can vary independently of scroll; neither is proof until
 * reconstructed native pixels qualify against the actual visible frame. */
unsigned cviii_camera_seeds_5h4(unsigned stage,unsigned area,unsigned room,
     int vertical,unsigned room_length,unsigned camera_ram,int ppu_scroll,
     CviiiCameraSeed5H4 seeds[3]);
const char *cviii_camera_gate_name_5h4(unsigned gate);
#endif
