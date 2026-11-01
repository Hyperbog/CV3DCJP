/* 5H-3 read-only camera/room evidence. No NES hardware mutations. */
#ifndef CVIII_CAMERA_TRACE_5H3_H
#define CVIII_CAMERA_TRACE_5H3_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint64_t frame;
    uint8_t stage, area, room;
    uint16_t camera_ram;
    int ppu_x, ppu_y;
    uint16_t ppu_bg_table;
    int qualified;
    uint32_t camera_x, camera_y;
    unsigned top, tested, matched;
    unsigned world_placed;
    unsigned gate_5h4, best_tested_5h4, best_matched_5h4;
    unsigned best_x_5h4, best_y_5h4, best_top_5h4, anchor_5h4;
    int chr_resolved_5h4;
    int32_t world_camera_x_5h5, world_best_x_5h5;
    unsigned world_crossed_5h5, world_active_5h5;
    int source; /* 0=OFF, 1=PACK, 2=FORCED_PPU, 3=MATTE, 4=CHECKED_PPU, 5=ATLAS, 6=LOCAL */
} CviiiCameraObservation5H3;

typedef struct {
    FILE *file;
    uint64_t last_frame;
    uint64_t samples, transitions, qualified;
    uint8_t last_stage, last_area, last_room;
    bool primed;
} CviiiCameraTrace5H3;

bool cviii_camera_trace_open_5h3(CviiiCameraTrace5H3 *trace,const char *path);
void cviii_camera_trace_close_5h3(CviiiCameraTrace5H3 *trace);
/* A frame is logged at most once, regardless of repeated renderer presents. */
bool cviii_camera_trace_append_5h3(CviiiCameraTrace5H3 *trace,
                                    const CviiiCameraObservation5H3 *row);
#endif
