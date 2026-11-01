#include "cviii_camera_trace_5h3.h"
#include "cviii_camera_gate_5h4.h"
#include <inttypes.h>
#include <string.h>

bool cviii_camera_trace_open_5h3(CviiiCameraTrace5H3 *t,const char *path)
{
    if(!t || !path || !*path || t->file)return false;
    FILE *f=fopen(path,"w");
    if(!f)return false;
    if(fputs("frame,stage,area,room,transition,camera_ram,ppu_x,ppu_y,ppu_bg_table,qualified,camera_x,camera_y,top,tested,matched,world_placed,source,gate_5h4,gate_label,best_tested,best_matched,best_x,best_y,best_top,anchor,chr_resolved,world_camera_x_5h5,world_best_x_5h5,world_crossed_5h5,world_active_5h5\n",f)==EOF) {
        fclose(f);return false;
    }
    memset(t,0,sizeof *t);
    t->file=f;
    return true;
}
void cviii_camera_trace_close_5h3(CviiiCameraTrace5H3 *t)
{
    if(!t)return;
    if(t->file)fclose(t->file);
    t->file=NULL;
}
bool cviii_camera_trace_append_5h3(CviiiCameraTrace5H3 *t,
                                    const CviiiCameraObservation5H3 *o)
{
    if(!t || !t->file || !o || (t->primed && o->frame<=t->last_frame) ||
       o->stage>=15u ||
       o->top>240u || o->matched>o->tested ||
       o->qualified<0 || o->qualified>1 || o->world_placed>1u ||
       o->source<0 || o->source>8 ||
       o->gate_5h4>CVIII_GATE_DYNAMIC_PRESENTED ||
       o->best_matched_5h4>o->best_tested_5h4 ||
       o->anchor_5h4>CVIII_ANCHOR_PAGE_PROBE ||
       o->chr_resolved_5h4<0 || o->chr_resolved_5h4>1 || o->world_active_5h5>1)return false;
    unsigned transition=(t->primed && (o->stage!=t->last_stage ||
        o->area!=t->last_area || o->room!=t->last_room))?1u:0u;
    /* Do not label a transition "verified": it is just an observation. */
    if(fprintf(t->file,"%" PRIu64 ",%u,%u,%u,%u,%u,%d,%d,%u,%d,%u,%u,%u,%u,%u,%u,%d,%u,%s,%u,%u,%u,%u,%u,%u,%d,%d,%d,%u,%u\n",
       o->frame,(unsigned)o->stage,(unsigned)o->area,(unsigned)o->room,
       transition,(unsigned)o->camera_ram,o->ppu_x,o->ppu_y,
       (unsigned)o->ppu_bg_table,o->qualified,o->camera_x,o->camera_y,
       o->top,o->tested,o->matched,o->world_placed,o->source,
       o->gate_5h4,cviii_camera_gate_name_5h4(o->gate_5h4),
       o->best_tested_5h4,o->best_matched_5h4,
       o->best_x_5h4,o->best_y_5h4,o->best_top_5h4,
       o->anchor_5h4,o->chr_resolved_5h4,
       o->world_camera_x_5h5,o->world_best_x_5h5,
       o->world_crossed_5h5,o->world_active_5h5)<0)return false;
    t->primed=true;
    t->last_frame=o->frame;
    t->last_stage=o->stage;t->last_area=o->area;t->last_room=o->room;
    t->samples++;t->transitions+=transition;t->qualified+=o->qualified!=0;
    /* Makes trace useful after an unexpected crash without affecting emulation. */
    if(transition)fflush(t->file);
    return true;
}
