/* JP0609: trace original Japanese PRG26 renderer input, not a replacement.
 * Unmodified CPU RAM object arrays and emulated physical OAM, every frame.
 * Eight object tables: base $0400, $041C, $0438, $0454, $0470, $048C,
 * $04A8 and OAM generation phase $22; seven tables total.
 * Original game can rotate slot order by $44 and stop at 64 sprites.
 * The trace never changes NES machine memory, timing or the frame.
 */
#include "cviii_object_oam_trace_0609.h"
#include <inttypes.h>
#include <string.h>

static const unsigned offsets[] = {0x0400u, 0x041cu, 0x0438u, 0x0454u,
                                    0x0470u, 0x048cu, 0x04a8u};
static const char *const columns[] = {"frame_offsets","base_y","base_x",
    "attributes","flags","selector","mirror"};

static void as_hex(FILE *out, const uint8_t *bytes, unsigned size)
{
    static const char digits[] = "0123456789abcdef";
    for (unsigned i=0;i<size;++i) {
        fputc(digits[bytes[i] >> 4],out);
        fputc(digits[bytes[i] & 15u],out);
    }
}

bool cviii_object_oam_trace_open(CviiiObjectOamTrace *trace,const char *path)
{
    if (!trace || !path || !*path) return false;
    FILE *file=fopen(path,"w");
    if (!file) return false;
    cviii_object_oam_trace_close(trace);
    memset(trace,0,sizeof(*trace));
    trace->out=file;
    trace->prev_phase=-1;
    fputs("frame,phase,phase_delta,active_objects,visible_oam",file);
    for (unsigned i=0;i<sizeof(columns)/sizeof(columns[0]);++i)
        fprintf(file,",%s",columns[i]);
    fputs(",ppu_oam\n",file);
    return true;
}
void cviii_object_oam_trace_close(CviiiObjectOamTrace *trace)
{
    if (!trace)return;
    if (trace->out)fclose(trace->out);
    trace->out=NULL;
}

void cviii_object_oam_trace_frame(CviiiObjectOamTrace *trace,
                                  const uint8_t *ram,const uint8_t *oam)
{
    if (!trace || !trace->out || !ram || !oam) return;
    unsigned active=0, visible=0;
    for(unsigned i=0;i<CVIII_OBJECTS;++i)
        active += (ram[0x0400u+i]!=0 && (ram[0x0470u+i]&0x80u)==0);
    for(unsigned i=0;i<64;++i)
        visible += oam[i*4] < 0xefu;
    int phase=ram[0x22u];
    int delta=trace->prev_phase<0 ? -1 : (phase-trace->prev_phase)&255;
    if(delta>0) {
        ++trace->phase_moves;
        if(delta==0x44)++trace->phase_44_moves;
    }
    trace->prev_phase=phase;
    ++trace->frames;
    trace->active_objects+=active;
    trace->visible_sprites+=visible;
    if(visible==64u)++trace->full_oam_frames;
    fprintf(trace->out,"%" PRIu64 ",%d,%d,%u,%u",
            trace->frames,phase,delta,active,visible);
    for(unsigned t=0;t<sizeof(offsets)/sizeof(offsets[0]);++t) {
        fputc(',',trace->out);
        as_hex(trace->out,ram+offsets[t],CVIII_OBJECTS);
    }
    fputc(',',trace->out);
    as_hex(trace->out,oam,CVIII_OAM_BYTES);
    fputc('\n',trace->out);
    if((trace->frames%60u)==0u)fflush(trace->out);
}
