/* 5H-6: frame-end read-only samples of per-scanline PPU state.
   IMPORTANT: CHR bank IDs here are *frame-end* observations, NOT IRQ history. */
#ifndef CVIII_PPU_AUDIT_5H6_H
#define CVIII_PPU_AUDIT_5H6_H
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
typedef struct {
    int (*scroll_x)(int), (*scroll_y)(int);
    uint16_t (*bg_table)(int);
    uint8_t (*mask)(int);
} CviiiPpuProbe5H6;
typedef struct {
    unsigned horizontal_changes, vertical_phase_breaks, pattern_changes, mask_changes;
    int x119,x120,x121,y119,y120,y121;
    unsigned bg119,bg120, mask119,mask120;
} CviiiPpuSummary5H6;
typedef struct {FILE *file;uint64_t last_frame;bool primed;} CviiiPpuAudit5H6;
/* Read-only, no guest writes and no inferred historical CHR registers. */
bool cviii_ppu_probe_5h6(const CviiiPpuProbe5H6 *p,CviiiPpuSummary5H6 *out);
bool cviii_ppu_open_5h6(CviiiPpuAudit5H6 *t,const char *path);
void cviii_ppu_close_5h6(CviiiPpuAudit5H6 *t);
bool cviii_ppu_append_5h6(CviiiPpuAudit5H6 *t,uint64_t frame,
    unsigned stage,unsigned area,unsigned room,unsigned gate,unsigned qualified,
    const CviiiPpuSummary5H6 *p,const uint8_t banks[4],bool banks_known);
/* Boundary-specific Y correction, only for source-verified room 0/1/0.
   All candidates MUST still satisfy the original >=96% pixel parity gate. */
unsigned cviii_ppu_camera_tops_5h6(unsigned stage,unsigned area,unsigned room,
                                  int y120,unsigned out[8]);
#endif
