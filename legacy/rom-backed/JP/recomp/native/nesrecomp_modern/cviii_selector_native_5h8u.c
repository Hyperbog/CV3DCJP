/* Development-only evidence. No CPU/PPU/ROM behavior changed. */
#include "cviii_selector_native_5h8u.h"
#include "cviii_room_diag.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static FILE *file_5h8u;
static uint64_t seq_5h8u;
static void record_5h8u(const uint64_t *v)
{
    if (!file_5h8u) return;
    fprintf(file_5h8u,"%llu,%llu",(unsigned long long)cviii_room_diag_frame_id(),
            (unsigned long long)++seq_5h8u);
    for (unsigned i=0;i<21;i++) {
        if (i==4) fprintf(file_5h8u,",%lld",(long long)(int64_t)v[i]);
        else fprintf(file_5h8u,",%llu",(unsigned long long)v[i]);
    }
    fputc('\n',file_5h8u);
    fflush(file_5h8u);
}
void cviii_selector_trace_power_on_5h8u(void)
{
    const char *path=getenv("CVIII_SELECTOR_TRACE_5H8U");
    cyc_cviii_selector_write_5h8u=NULL;
    if (!path || !*path) return;
    if (!file_5h8u) {
        file_5h8u=fopen(path,"wb");
        if (!file_5h8u) { fprintf(stderr,"5H8U: could not open trace path\n"); return; }
        fputs("capture_frame,sequence,fetch_epoch,fetch_cycle,write_cycle,pc,prg_offset,"
              "raw_address,address,old_value,new_value,fetch_a,fetch_x,fetch_y,"
              "write_a,write_x,write_y,zp00,zp01,stage_before,area_before,room_before,epoch_open\n",
              file_5h8u);
    }
    cyc_cviii_selector_write_5h8u=record_5h8u;
}
