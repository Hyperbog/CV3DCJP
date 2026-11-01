/* Phase 5G: make the requested viewport observable and render margins.
 * Authentic room caches are preferred in auto mode; missing/invalid room
 * selectors fall back to current PPU nametable/CHR/palette read-only projection.
 * CHR banks and palette may change mid-scanline: reconstructed margins are
 * EXPERIMENTAL, NOT an original-Mesen-verified image or offscreen simulation.
 */
#include "cviii_room_diag.h"
#include "cviii_reference_capture_5h8l2.h"
#include "cviii_chr_index_0598.h"
#include "cviii_whole_room.h"
#include "cviii_room_gfx.h"
#include "cviii_room_renderer.h"
#include "cviii_room_preview.h"
#include "cviii_ppu_guard_5g2.h"
#include "cviii_native_world_5h2.h"
#include "cviii_camera_trace_5h3.h"
#include "cviii_camera_gate_5h4.h"
#include "cviii_world_parity_5h5.h"
#include "cviii_ppu_audit_5h6.h"
#include "cviii_camera_track_5h7.h"
#include "cviii_room_local_5h8.h"
#include "cviii_scene_epoch_5h8c.h"
#include "cviii_dual_scene_5h8c.h"
#include "cviii_transition_5h8d.h"
#include "cviii_boss_cache_5h8c.h"
#include "cviii_mutable_tiles_5h8e.h"
#include "cviii_entity_events_5h8f.h"
#include "cyc_core.h"
#include "cyc_render.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NATIVE_W 256u
#define HEIGHT 240u
#define COMPARE_W 512u
#define WIDE_W 426u
#define MARGIN ((WIDE_W-NATIVE_W)/2u)
#define MATTE UINT32_C(0xff000000)

enum { SOURCE_AUTO=0, SOURCE_ROOM=1, SOURCE_PPU=2, SOURCE_ATLAS=3 };
enum { OUTPUT_NONE=0, OUTPUT_ROOM=1, OUTPUT_PPU=2, OUTPUT_MATTE=3, OUTPUT_VERIFIED_PPU=4, OUTPUT_ATLAS=5, OUTPUT_LOCAL=6, OUTPUT_DUAL=7, OUTPUT_DYNAMIC=8 };
typedef struct {
    CviiiChrIndex chr;
    CviiiWholeRoomPack rooms;
    CviiiRoomGfx gfx;
    CviiiNativeWorld5H2 world;
    CviiiCameraTrace5H3 trace;
    CviiiPpuAudit5H6 ppu_trace;
    FILE *scene_trace;
    CviiiCameraTrack5H7 camera_5h7;
    CviiiLocalTracker5H8 local_track;
    CviiiSceneEpoch5H8C scene_epoch;
    CviiiSceneEvent5H8C scene_event;
    CviiiBossCache5H8C boss_cache;
    CviiiTransitionTrack5H8D transition_5h8d;
    CviiiMutableTiles5H8E mutable_5h8e;
    CviiiEntityLedger5H8F entities_5h8f;
    FILE *entities_trace;
    FILE *mutations_trace;
    struct {
        CviiiRoomRenderSource source;
        CviiiWholeRoomView room;
        uint64_t frame;
        unsigned x,y,top;
        int scroll_x,scroll_y;
        bool valid;
    } last_verified;
    bool local_active;
    uint64_t frame_seq;
    unsigned matches, tested;
    unsigned gate, best_tested, best_matched, best_x, best_y, best_top, anchor;
    int chr_resolved;
    char world_path[1024];
    int world_attempted;
    unsigned camera_top;
    int32_t world_camera_x, best_world_x;
    unsigned world_crossed;
    bool world_camera_active;
    CviiiWholeRoomView active;
    int valid;
    int mode;
    int source;
    int last_report_mode;
    int last_report_source;
    int report_initialized;
    CviiiPpuResult5G2 last_guard;
    unsigned last_camera_x,last_camera_y;
    uint8_t last_banks[4];
    uint32_t image[COMPARE_W*HEIGHT];
} RoomState;
static RoomState state;

bool cviii_room_diag_mode(int mode)
{
    if (mode<CVIII_ROOM_OFF || mode>CVIII_ROOM_EXTENDED) return false;
    if (state.mode!=mode) state.report_initialized=0;
    state.mode=mode;
    return true;
}
int cviii_room_diag_get_mode(void) { return state.mode; }
bool cviii_room_diag_source(const char *s)
{
    int value;
    if (!s) return false;
    if (!strcmp(s,"auto")) value=SOURCE_AUTO;
    else if (!strcmp(s,"room")) value=SOURCE_ROOM;
    else if (!strcmp(s,"ppu")) value=SOURCE_PPU;
    else if (!strcmp(s,"atlas")) value=SOURCE_ATLAS;
    else return false;
    if (state.source!=value) state.report_initialized=0;
    state.source=value;
    return true;
}
const char *cviii_room_diag_get_source(void)
{
    return state.source==SOURCE_ROOM?"room":state.source==SOURCE_PPU?"ppu":
           state.source==SOURCE_ATLAS?"atlas":"auto";
}
bool cviii_room_diag_load_chr(const char *p)
{
    if (!p) return false;
    bool ok=cviii_chr_index_load(&state.chr,p);
    state.valid=0;
    state.world.loaded=false; state.world_attempted=0;
    state.report_initialized=0;
    return ok;
}
bool cviii_room_diag_load_rooms(const char *p)
{
    if (!p) return false;
    bool ok=cviii_whole_room_open(&state.rooms,p);
    state.valid=0;
    state.world.loaded=false; state.world_attempted=0;
    state.report_initialized=0;
    return ok;
}
bool cviii_room_diag_load_graphics(const char *p)
{
    if (!p) return false;
    bool ok=cviii_room_gfx_open(&state.gfx,p);
    state.valid=0;
    state.world.loaded=false; state.world_attempted=0;
    state.report_initialized=0;
    return ok;
}
bool cviii_room_diag_index(const char *path)
{
    if(!path || !*path || strlen(path)>=sizeof state.world_path)return false;
    FILE *f=fopen(path,"rb");if(!f)return false;fclose(f);
    memcpy(state.world_path,path,strlen(path)+1);
    state.world.loaded=false;state.world_attempted=0;
    state.report_initialized=0;
    return true;
}
bool cviii_room_diag_trace(const char *path)
{
    if(!path||!cviii_camera_trace_open_5h3(&state.trace,path))return false;
    char sibling[1200];
    size_t n=strlen(path);
    if(n+20>=sizeof sibling){fprintf(stderr,"CVIII-5H6: PPU trace path too long\n");return true;}
    memcpy(sibling,path,n+1);
    char *dot=strrchr(sibling,'.');
    if(dot && !strcmp(dot,".csv"))strcpy(dot,"-ppu.csv");
    else strcat(sibling,"-ppu.csv");
    if(!cviii_ppu_open_5h6(&state.ppu_trace,sibling))
        fprintf(stderr,"CVIII-5H6: cannot open scanline audit: %s\n",sibling);
    else fprintf(stderr,"CVIII-5H6: scanline audit -> %s (CHR registers: frame-end only)\n",sibling);
    memcpy(sibling,path,n+1);
    dot=strrchr(sibling,'.');
    if(dot && !strcmp(dot,".csv"))strcpy(dot,"-scene.csv");
    else strcat(sibling,"-scene.csv");
    state.scene_trace=fopen(sibling,"w");
    if(state.scene_trace)fputs("frame,stage,area,room,scene_epoch,revision,event_flags,ciram_changed\n",state.scene_trace);
    memcpy(sibling,path,n+1);
    dot=strrchr(sibling,'.');
    if(dot && !strcmp(dot,".csv"))strcpy(dot,"-entities.csv");
    else strcat(sibling,"-entities.csv");
    state.entities_trace=fopen(sibling,"w");
    if(state.entities_trace)fputs("frame,stage,area,room,entity_id,kind,state,scope,producer_pc\n",state.entities_trace);
    memcpy(sibling,path,n+1);
    dot=strrchr(sibling,'.');
    if(dot && !strcmp(dot,".csv"))strcpy(dot,"-mutations.csv");
    else strcat(sibling,"-mutations.csv");
    state.mutations_trace=fopen(sibling,"w");
    if(state.mutations_trace)fputs("frame,stage,area,room,confirmed_tiles,margin_pixels\n",state.mutations_trace);
    return true;
}
void cviii_room_diag_boundary(void)
{
    state.valid=0; state.report_initialized=0;
    cviii_camera_track_reset_5h7(&state.camera_5h7);
    cviii_local_reset_5h8(&state.local_track);
    cviii_scene_epoch_reset_5h8c(&state.scene_epoch);
    cviii_boss_cache_clear_5h8c(&state.boss_cache);
    cviii_transition_reset_5h8d(&state.transition_5h8d);
    cviii_mut_reset_5h8e(&state.mutable_5h8e);
    cviii_entity_reset_5h8f(&state.entities_5h8f);
    state.last_verified.valid=false;
    /* Keep trace file across power-on/state-load, without inventing a
     * frame number or rewriting prior observations. */
}
void cviii_room_diag_frame_end(void)
{
    state.frame_seq++;
    state.valid=0;
    state.gate=CVIII_GATE_NOT_EVALUATED;
    if(state.world_path[0] && !state.world_attempted &&
       state.rooms.allocation && state.gfx.allocation) {
        state.world_attempted=1;
        if(!cviii_world_index_open_5h2(&state.world,state.world_path,
                                       &state.rooms,&state.gfx))
            fprintf(stderr,"CVIII-5H2: rejected world index (provenance/layout/edge mismatch)\n");
        else fprintf(stderr,"CVIII-5H2: loaded private indexed world, 135 room IDs, 2 placed\n");
    }
    if (!state.mode || !state.rooms.allocation || !state.gfx.allocation ||
        !state.chr.pixels) return;
    const uint8_t *ram=cyc_cpu_ram();
    if (!ram) return;
    if (!cviii_whole_room_find(&state.rooms,ram[0x34],ram[0x35],
                               ram[0x36],&state.active)) return;
    state.valid=cviii_room_gfx_group(&state.gfx,&state.active)!=NULL;
    cviii_mut_visit_5h8e(&state.mutable_5h8e,
         ram[0x34],ram[0x35],ram[0x36]);
    cviii_entity_visit_5h8f(&state.entities_5h8f,
         ram[0x34],ram[0x35],ram[0x36]);
    /* Hardware-only scene identity, sampled after the machine frame. */
    size_t ciram_len=0;
    const uint8_t *ciram=cyc_ppu_ciram(&ciram_len);
    const uint8_t *pal=cyc_ppu_palette();
    if(ciram && ciram_len==2048 && pal){
        uint32_t h=2166136261u;
        for(unsigned a=0;a<8192;a++)h=(h^cyc_render_chr((uint16_t)a))*16777619u;
        if(cviii_scene_epoch_observe_5h8c(&state.scene_epoch,state.frame_seq,
               ram[0x34],ram[0x35],ram[0x36],ciram,ciram_len,pal,h,
               &state.scene_event)) {
            if(state.scene_trace){
                fprintf(state.scene_trace,"%llu,%u,%u,%u,%u,%u,%u,%u\n",
                    (unsigned long long)state.frame_seq,ram[0x34],ram[0x35],ram[0x36],
                    state.scene_event.epoch,state.scene_event.revision,
                    state.scene_event.flags,state.scene_event.ciram_changed);
                if(state.scene_event.flags)fflush(state.scene_trace);
            }
            if(state.scene_event.flags&
                (CVIII_EPOCH_ROOM|CVIII_EPOCH_MAJOR_CIRAM|CVIII_EPOCH_CHR|
                 CVIII_EPOCH_PALETTE|CVIII_EPOCH_DISCONTINUITY))
                cviii_boss_cache_clear_5h8c(&state.boss_cache);
            /* Old tile colors are unsafe after a graphics/scene revision. */
            if(state.scene_event.flags&
                (CVIII_EPOCH_MAJOR_CIRAM|CVIII_EPOCH_CHR|CVIII_EPOCH_PALETTE|
                 CVIII_EPOCH_DISCONTINUITY)){
                cviii_mut_reset_5h8e(&state.mutable_5h8e);
                cviii_mut_visit_5h8e(&state.mutable_5h8e,
                    ram[0x34],ram[0x35],ram[0x36]);
            }
        }
    }
}
void cviii_room_diag_center_copy(uint32_t *dst,unsigned width,
                                unsigned height,const uint32_t *src,uint32_t matte)
{
    if (!dst || !src || width<NATIVE_W || width>COMPARE_W || height!=HEIGHT) return;
    for (unsigned y=0;y<HEIGHT;++y) {
        uint32_t *row=dst+(size_t)y*width;
        for (unsigned x=0;x<width;++x) row[x]=matte;
        memcpy(row+(width-NATIVE_W)/2u,src+(size_t)y*NATIVE_W,
               NATIVE_W*sizeof(uint32_t));
    }
}
/* Uses the PPU's current mapped nametable, current CHR and palette.
 * The per-line scroll position is sampled from the frame that was drawn;
 * the current mapper state is not a historical per-scanline CHR snapshot. */
static uint32_t live_ppu_background(unsigned y,int native_x)
{
    if (!(cyc_render_line_mask((int)y)&0x08u)) return cyc_render_color(0);
    int wx=(cyc_render_line_scroll_x((int)y)+native_x)%512;
    if (wx<0) wx+=512;
    int wy=cyc_render_line_scroll_y((int)y)%480;
    if (wy<0) wy+=480;
    const unsigned nt=(unsigned)(wy/240)*2u+(unsigned)(wx/256);
    const unsigned local_x=(unsigned)wx&255u;
    const unsigned local_y=(unsigned)wy%240u;
    const uint16_t base=(uint16_t)(0x2000u+nt*0x400u);
    const uint16_t tile_address=(uint16_t)(base+(local_y>>3u)*32u+(local_x>>3u));
    const uint8_t tile=cyc_render_nametable(tile_address);
    const uint16_t attr_address=(uint16_t)(base+0x3c0u+(local_y>>5u)*8u+(local_x>>5u));
    const unsigned shift=(((local_y>>4u)&1u)*4u)+(((local_x>>4u)&1u)*2u);
    const unsigned pal=(cyc_render_nametable(attr_address)>>shift)&3u;
    const uint16_t pattern=(uint16_t)(cyc_render_line_bg_table((int)y)+tile*16u+(local_y&7u));
    const uint8_t low=cyc_render_chr(pattern);
    const uint8_t high=cyc_render_chr((uint16_t)(pattern+8u));
    const unsigned bit=7u-(local_x&7u);
    const unsigned pixel=((low>>bit)&1u)|(((high>>bit)&1u)<<1u);
    return cyc_render_color(pixel?(int)(pal*4u+pixel):0);
}
static void paint_ppu_margin(uint32_t *dst,unsigned out_w,unsigned y,unsigned from,
                             unsigned count,int native_x0)
{
    for (unsigned x=0;x<count;++x)
        dst[(size_t)y*out_w+from+x]=live_ppu_background(y,native_x0+(int)x);
}
/* Infer actual *end-of-frame* mapper windows from the authenticated CHR cache.
 * Never silently substitute room-start R4..R7 bank guesses. This is not a
 * mid-frame CHR trace, so all reconstructed room pixels still need to agree
 * with the real visible frame before the source is admitted. */
static bool chr_live_banks(uint8_t out[4])
{
    if (!state.chr.raw) return false;
    const uint16_t base=cyc_render_line_bg_table(120);
    if(base!=0 && base!=0x1000u) return false;
    static const unsigned probe[]={0,1,7,15,17,41,127,255,511,767,1009,1023};
    for(unsigned win=0;win<4u;win++) {
        uint16_t ppu=(uint16_t)(base+win*1024u);
        uint8_t observed[sizeof probe/sizeof probe[0]];
        for(unsigned k=0;k<sizeof probe/sizeof probe[0];k++)
            observed[k]=cyc_render_chr((uint16_t)(ppu+probe[k]));
        unsigned chosen=128;
        for(unsigned bank=0;bank<128u;bank++) {
            const uint8_t *chunk=state.chr.raw+bank*1024u;
            bool candidate=true;
            for(unsigned k=0;k<sizeof probe/sizeof probe[0];k++)
                if(chunk[probe[k]]!=observed[k]){candidate=false;break;}
            if(!candidate)continue;
            for(unsigned i=0;i<1024;i++)
                if(chunk[i]!=cyc_render_chr((uint16_t)(ppu+i))) {
                    candidate=false; break;
                }
            if(candidate){chosen=bank;break;}
        }
        if(chosen==128u)return false;
        out[win]=(uint8_t)chosen;
    }
    return true;
}
static bool over_actor(const uint8_t *oam,unsigned x,unsigned y)
{
    if(!oam)return false;
    for(unsigned slot=0;slot<64u;slot++) {
        const uint8_t *o=oam+slot*4u;
        if(o[0]>=0xefu)continue;
        unsigned sy=(unsigned)o[0]+1u;
        if(x>=o[3] && x<(unsigned)o[3]+8u && y>=sy && y<sy+16u)
            return true;
    }
    return false;
}
/* Stage-1 indexed-world parity. A transition can show pixels from BOTH
 * verified rooms even though the active selector has already changed.
 * Only a fully pixel-qualified global camera can reach the margin painter. */
static bool resolve_world_camera(const CviiiRoomRenderSource *src,
                                 unsigned *cx,unsigned *cy)
{
    const CviiiWorldEntry5H2 *room=cviii_world_index_find_5h2(&state.world,
        src->room->stage,src->room->area,src->room->room);
    const uint8_t *ram=cyc_cpu_ram();
    const uint32_t *original=cyc_frame_argb();
    if(!ram||!original){state.gate=CVIII_GATE_RAM_MISSING;return false;}
    if(!room||!room->placed){state.gate=CVIII_GATE_WORLD_UNPLACED;return false;}
    unsigned approx=(unsigned)ram[0x56] | ((unsigned)ram[0x57]<<8u);
    bool vertical=(src->room->metadata&0x80u)!=0;
    unsigned length=(vertical?src->room->height:src->room->width)*32u;
    CviiiCameraSeed5H4 local[3]={0};
    unsigned n=cviii_camera_seeds_5h4(src->room->stage,src->room->area,
        src->room->room,vertical,length,approx,
        cyc_render_line_scroll_x(120),local);
    /* Stage 1 starts with 768px of verified room geometry but scroll X
     * wraps at 512. Track the page; never use it without pixel parity. */
    const bool starting=!vertical && src->room->stage==0 &&
        src->room->area==0 && src->room->room==0;
    int32_t seeds[9]={0};unsigned sources[9]={0};unsigned count=0;
    int32_t track_seeds[3]={0};
    unsigned tn=cviii_camera_track_propose_5h7(&state.camera_5h7,
        state.frame_seq,src->room->stage,src->room->area,src->room->room,
        cyc_render_line_scroll_x(120),length,track_seeds);
    for(unsigned i=0;i<tn;i++) {
        seeds[count]=room->x+track_seeds[i];
        sources[count++]=(i==0?CVIII_ANCHOR_TRACKED_SCROLL:CVIII_ANCHOR_PAGE_PROBE); /* 5H7 tracked or page probe */
    }
    int32_t seam;
    if(!vertical && cviii_world_seam_candidate_5h5(&state.world,src->room,
           cyc_render_line_scroll_x(120),&seam)) {
        seeds[count]=seam;sources[count++]=CVIII_ANCHOR_PPU_SCROLL;
    }
    for(unsigned i=0;i<n && count<9;i++) {
        int64_t global=vertical?(int64_t)room->x:
                                (int64_t)room->x+local[i].pixel;
        if(global>=INT32_MIN && global<=INT32_MAX) {
            seeds[count]=(int32_t)global;sources[count++]=local[i].source;
        }
    }
    static const int offsets[]={0,1,-1,2,-2,4,-4,8,-8,16,-16,32,-32};
    unsigned tops[8];
    const unsigned top_count=cviii_ppu_camera_tops_5h6(src->room->stage,
        src->room->area,src->room->room,cyc_render_line_scroll_y(120),tops);
    unsigned best_tested=0,best_matched=0;
    unsigned valid_tested=0,valid_matched=0,valid_crossed=0;
    int32_t valid_world=0;unsigned valid_top=0,valid_anchor=0;
    int32_t best_world=0;
    const uint8_t *oam=cyc_render_oam();
    for(unsigned i=0;i<count;i++)for(unsigned k=0;
        k<sizeof offsets/sizeof offsets[0];k++) {
        int64_t camera=(int64_t)seeds[i]+offsets[k];
        if(camera<0||camera>INT32_MAX)continue;
        if(starting && !cviii_camera_track_allowed_5h7(
            &state.camera_5h7,(int32_t)camera-room->x))continue;
        for(unsigned t=0;t<top_count;t++) {
            unsigned top=tops[t];
            CviiiWorldMatch5H5 m=cviii_world_match_5h5(&state.world,&state.rooms,
                src,camera,room->y,top,original,oam);
            if(m.tested && (!best_tested ||
                (uint64_t)m.matched*best_tested>(uint64_t)best_matched*m.tested ||
                ((uint64_t)m.matched*best_tested==(uint64_t)best_matched*m.tested &&
                   m.tested>best_tested))) {
                best_tested=m.tested;best_matched=m.matched;
                best_world=(int32_t)camera;
                state.best_x=(unsigned)(camera<room->x?0:camera-room->x);
                state.best_y=0;state.best_top=top;state.anchor=sources[i];
            }
            if(m.tested>=90 && (uint64_t)m.matched*100u>=(uint64_t)m.tested*96u &&
               (m.tested>valid_tested ||
                (m.tested==valid_tested && m.matched>valid_matched))) {
                valid_tested=m.tested;valid_matched=m.matched;
                valid_world=(int32_t)camera;valid_top=top;
                valid_crossed=m.crossed;valid_anchor=sources[i];
            }
        }
    }
    state.best_world_x=best_world;
    state.best_tested=best_tested;state.best_matched=best_matched;
    state.tested=valid_tested;state.matches=valid_matched;
    if(valid_tested<90){state.gate=CVIII_GATE_CAMERA_MISMATCH;return false;}
    state.world_camera_active=true;
    state.world_camera_x=valid_world;state.world_crossed=valid_crossed;
    state.camera_top=valid_top;state.anchor=valid_anchor;
    if(starting)cviii_camera_track_confirm_5h7(&state.camera_5h7,
                                                 valid_world-room->x);
    state.last_camera_x=valid_world<room->x?0u:(unsigned)(valid_world-room->x);
    state.last_camera_y=0;
    memcpy(state.last_banks,src->bg_bank_1k,4);
    *cx=state.last_camera_x;*cy=0;
    return true;
}
/* Unplaced rooms are local-only: no world edges, no invented neighbors.
 * The visible NES image is the authority. Reject every candidate without
 * at least 90 non-sprite samples and 96% exact color agreement. */
static bool resolve_local_camera(CviiiRoomRenderSource *src,
                                  unsigned *cx,unsigned *cy)
{
    const uint8_t *ram=cyc_cpu_ram();
    const uint32_t *native=cyc_frame_argb();
    if(!ram||!native){state.gate=CVIII_GATE_RAM_MISSING;return false;}
    const bool vertical=(src->room->metadata&0x80u)!=0;
    const int ppu_sample=vertical?cyc_render_line_scroll_y(120):
                                   cyc_render_line_scroll_x(120);
    const unsigned hint=(unsigned)ram[0x56]|((unsigned)ram[0x57]<<8u);
    const unsigned extent=(vertical?src->room->height:src->room->width)*32u;
    static const int offsets[]={0,1,-1,2,-2,4,-4,8,-8,16,-16,32,-32};
    unsigned tops[8];
    const unsigned count_top=cviii_ppu_camera_tops_5h6(src->room->stage,
          src->room->area,src->room->room,cyc_render_line_scroll_y(120),tops);
    unsigned valid_n=0,valid_match=0,valid_x=0,valid_y=0,valid_top=0;
    unsigned best_n=0,best_match=0;
    const uint8_t *oam=cyc_render_oam();
    for(unsigned t=0;t<count_top;t++){
        /* cyc_render_line_scroll_y(120) is the source position *at* line
         * 120. Undo its displacement from gameplay top before trying pages.
         * Never assume screen Y is itself a world-camera origin. */
        const unsigned top=tops[t];
        int scroll=ppu_sample;
        if(vertical){
            scroll=(scroll-((120-(int)top))+480)%480;
        }
        unsigned seeds[8]={0};
        const unsigned n=cviii_local_seeds_5h8(&state.local_track,
                  state.frame_seq,src->room,scroll,hint,seeds);
        for(unsigned i=0;i<n;i++)for(unsigned k=0;k<
                  sizeof offsets/sizeof offsets[0];k++){
            const int64_t candidate=(int64_t)seeds[i]+offsets[k];
            if(candidate<0||candidate>=extent)continue;
            const unsigned x=vertical?0u:(unsigned)candidate;
            const unsigned y=vertical?(unsigned)candidate:0u;
            const CviiiLocalScore5H8 m=cviii_local_match_5h8(src,x,y,
                                               top,native,oam);
            if(m.tested && (!best_n ||
                (uint64_t)m.matched*best_n>(uint64_t)best_match*m.tested ||
                ((uint64_t)m.matched*best_n==(uint64_t)best_match*m.tested &&
                  m.tested>best_n))){
                best_n=m.tested;best_match=m.matched;
                state.best_x=x;state.best_y=y;state.best_top=top;
                state.anchor=i==0 && state.local_track.valid?
                      CVIII_ANCHOR_TRACKED_SCROLL:CVIII_ANCHOR_PPU_SCROLL;
            }
            if(m.tested>=90 && (uint64_t)m.matched*100>=
                                     (uint64_t)m.tested*96 &&
               (m.tested>valid_n ||
                (m.tested==valid_n&&m.matched>valid_match))){
                valid_n=m.tested;valid_match=m.matched;
                valid_x=x;valid_y=y;valid_top=top;
            }
        }
    }
    state.best_tested=best_n;state.best_matched=best_match;
    if(valid_n<90){state.gate=CVIII_GATE_LOCAL_CAMERA_MISMATCH;return false;}
    state.matches=valid_match;state.tested=valid_n;
    state.last_camera_x=*cx=valid_x;state.last_camera_y=*cy=valid_y;
    state.camera_top=valid_top;state.local_active=true;
    memcpy(state.last_banks,src->bg_bank_1k,4);
    const int valid_scroll=vertical?
       (ppu_sample-(120-(int)valid_top)+480)%480:ppu_sample;
    cviii_local_confirm_5h8(&state.local_track,state.frame_seq,src->room,
                             valid_scroll,vertical?valid_y:valid_x);
    return true;
}
/* Original RAM $56/$57 is a hypothesis, not a proof that the pixel camera
 * matches the room cache. Search bounded nearby offsets and require agreement
 * with original visible pixels outside sprite regions before painting margins. */
static bool resolve_camera(CviiiRoomRenderSource *src,unsigned *cx,unsigned *cy)
{
    if(state.world.loaded && state.mode==CVIII_ROOM_EXTENDED &&
       (state.source==SOURCE_ATLAS || state.source==SOURCE_AUTO)) {
        const CviiiWorldEntry5H2 *entry=cviii_world_index_find_5h2(
            &state.world,src->room->stage,src->room->area,src->room->room);
        if(entry && entry->placed)return resolve_world_camera(src,cx,cy);
        return resolve_local_camera(src,cx,cy);
    }
    const uint8_t *ram=cyc_cpu_ram();
    const uint32_t *original=cyc_frame_argb();
    if(!ram || !original){state.gate=CVIII_GATE_RAM_MISSING;return false;}
    unsigned approx=((unsigned)ram[0x57]<<8u)|ram[0x56];
    bool vertical=(src->room->metadata&0x80u)!=0;
    unsigned length=(vertical?src->room->height:src->room->width)*32u;
    CviiiCameraSeed5H4 seeds[3]={0};
    unsigned n=cviii_camera_seeds_5h4(src->room->stage,src->room->area,
         src->room->room,vertical,length,approx,
         cyc_render_line_scroll_x(120),seeds);
    static const int offset[]={0,1,-1,2,-2,4,-4,8,-8,16,-16,32,-32};
    unsigned top_candidates[8];
    const unsigned top_count=cviii_ppu_camera_tops_5h6(src->room->stage,
        src->room->area,src->room->room,cyc_render_line_scroll_y(120),top_candidates);
    const uint8_t *oam=cyc_render_oam();
    unsigned qualified_tested=0,qualified_matched=0;
    unsigned qualified_x=0,qualified_y=0,qualified_top=0,qualified_anchor=0;
    unsigned highest_tested=0,highest_matched=0;
    for(unsigned seed=0;seed<n;seed++) {
      for(unsigned k=0;k<sizeof offset/sizeof offset[0];k++) {
        int camera=(int)seeds[seed].pixel+offset[k];
        if(camera<0 || (unsigned)camera>=length)continue;
        unsigned candidate_x=vertical?0u:(unsigned)camera;
        unsigned candidate_y=vertical?(unsigned)camera:0u;
        for(unsigned ti=0;ti<top_count;ti++) {
          unsigned top=top_candidates[ti];
          unsigned tested=0,matches=0;
          for(unsigned y=64;y<=224;y+=16u) {
            if(y<top)continue;
            for(unsigned x=16;x<=240;x+=16u) {
              if(over_actor(oam,x,y))continue;
              CviiiGfxPixel px; uint32_t expected;
              if(!cviii_room_gfx_sample(src->graphics,src->room,src->chr,
                    src->bg_bank_1k,candidate_x+x,candidate_y+y-top,&px) ||
                 !cviii_room_gfx_color(&px,src->palette_argb,&expected))continue;
              tested++;
              if(expected==original[(size_t)y*NATIVE_W+x])matches++;
            }
          }
          /* Report the highest available score even when the strict camera
           * gate rejects it. Never convert a failed match to a camera. */
          if(tested && (!highest_tested ||
             (uint64_t)matches*highest_tested>(uint64_t)highest_matched*tested ||
             ((uint64_t)matches*highest_tested==
              (uint64_t)highest_matched*tested && tested>highest_tested))) {
             highest_tested=tested;highest_matched=matches;
             state.best_x=candidate_x;state.best_y=candidate_y;
             state.best_top=top;state.anchor=seeds[seed].source;
          }
          if(tested>=90u && matches*100u>=tested*96u &&
             (tested>qualified_tested ||
              (tested==qualified_tested && matches>qualified_matched))) {
             qualified_tested=tested;qualified_matched=matches;
             qualified_x=candidate_x;qualified_y=candidate_y;
             qualified_top=top;qualified_anchor=seeds[seed].source;
          }
        }
      }
    }
    state.best_tested=highest_tested;state.best_matched=highest_matched;
    state.tested=qualified_tested;state.matches=qualified_matched;
    if(qualified_tested<90u){state.gate=CVIII_GATE_CAMERA_MISMATCH;return false;}
    *cx=qualified_x;*cy=qualified_y;
    state.last_camera_x=qualified_x;state.last_camera_y=qualified_y;
    state.camera_top=qualified_top;
    state.anchor=qualified_anchor;
    memcpy(state.last_banks,src->bg_bank_1k,4);
    return true;
}
static bool room_source(CviiiRoomRenderSource *out,unsigned *cx,unsigned *cy)
{
    if(!out || !cx || !cy || !state.valid) {
        if(!state.rooms.allocation || !state.gfx.allocation || !state.chr.pixels)
            state.gate=CVIII_GATE_ASSETS_MISSING;
        else if(!cyc_cpu_ram())state.gate=CVIII_GATE_RAM_MISSING;
        else {
            const uint8_t *ram=cyc_cpu_ram();
            CviiiWholeRoomView candidate;
            state.gate=cviii_whole_room_find(&state.rooms,ram[0x34],
                ram[0x35],ram[0x36],&candidate)?CVIII_GATE_GRAPHICS_UNKNOWN:
                CVIII_GATE_ROOM_UNKNOWN;
        }
        return false;
    }
    memset(out,0,sizeof(*out));
    out->graphics=&state.gfx;
    out->room=&state.active;
    out->chr=&state.chr;
    if(!chr_live_banks(out->bg_bank_1k)) {
        state.gate=CVIII_GATE_CHR_UNRESOLVED;return false;
    }
    state.chr_resolved=1;
    for(unsigned i=0;i<16u;++i)
        out->palette_argb[i]=cyc_render_color((int)i);
    if(!cviii_room_render_validate(out)) {
        state.gate=CVIII_GATE_SOURCE_INVALID;return false;
    }
    return resolve_camera(out,cx,cy);
}
static uint8_t ppu_nt(uint16_t a){return cyc_render_nametable(a);}
static uint8_t ppu_chr(uint16_t a){return cyc_render_chr(a);}
static uint32_t ppu_color(int i){return cyc_render_color(i);}
static int ppu_scroll_x(int y){return cyc_render_line_scroll_x(y);}
static int ppu_scroll_y(int y){return cyc_render_line_scroll_y(y);}
static uint16_t ppu_bg_table(int y){return cyc_render_line_bg_table(y);}
static uint8_t ppu_mask(int y){return cyc_render_line_mask(y);}
static CviiiPpuResult5G2 paint_verified_ppu(const uint32_t *native,unsigned w,unsigned origin)
{
    const CviiiPpuSource5G2 src={
        ppu_nt,ppu_chr,ppu_color,ppu_scroll_x,ppu_scroll_y,
        ppu_bg_table,ppu_mask,cyc_render_oam()
    };
    uint32_t painted=0;
    return cviii_ppu_verified_paint_5g2(&src,native,state.image,
                                        (int)w,(int)HEIGHT,(int)origin,&painted);
}
static void report(int mode,int output)
{
    if (state.report_initialized && state.last_report_mode==mode &&
        state.last_report_source==output) return;
    const char *kind=output==OUTPUT_ROOM?"ROOM_PACK":
                     output==OUTPUT_PPU?"LIVE_PPU_FORCED_APPROX":
                     output==OUTPUT_ATLAS?"AUTHENTIC_INDEXED_WORLD":
                     output==OUTPUT_LOCAL?"AUTHENTIC_ROOM_LOCAL":
                     output==OUTPUT_DUAL?"TWO_ROOM_PIXEL_MATCHED":
                     output==OUTPUT_DYNAMIC?"PRIOR_OBSERVED_BG_ONLY":
                     output==OUTPUT_VERIFIED_PPU?"LIVE_PPU_MATCHED":
                     output==OUTPUT_MATTE?"NO_ROOM": "OFF";
    fprintf(stderr,"CVIII-5G2: view=%d source=%s size=%ux%u "
                   "gate=%s (Original center unchanged; no offscreen actors)\n",
            mode,kind,mode==CVIII_ROOM_EXTENDED?WIDE_W:
                       mode==CVIII_ROOM_COMPARE?COMPARE_W:NATIVE_W,HEIGHT,
            output==OUTPUT_MATTE?cviii_ppu_result_name_5g2(state.last_guard):"n/a");
    if(output==OUTPUT_ROOM || output==OUTPUT_ATLAS || output==OUTPUT_LOCAL) fprintf(stderr,
        "CVIII-5H2: room camera=%u,%u top=%u CHR1k=%u,%u,%u,%u (NESRecomp-only parity)\n",
        state.last_camera_x,state.last_camera_y,state.camera_top,
        state.last_banks[0],state.last_banks[1],state.last_banks[2],state.last_banks[3]);
    state.last_report_mode=mode;
    state.last_report_source=output;
    state.report_initialized=1;
}
static int ppu_scroll_x(int y);
static int ppu_scroll_y(int y);
static uint16_t ppu_bg_table(int y);
static uint8_t ppu_mask(int y);
static void trace_presentation(int output,int qualified)
{
    if(!state.trace.file)return;
    const uint8_t *ram=cyc_cpu_ram();
    if(!ram)return;
    CviiiCameraObservation5H3 row={0};
    row.frame=state.frame_seq;
    row.stage=ram[0x34];row.area=ram[0x35];row.room=ram[0x36];
    row.camera_ram=(uint16_t)(ram[0x56]|((uint16_t)ram[0x57]<<8));
    row.ppu_x=cyc_render_line_scroll_x(120);
    row.ppu_y=cyc_render_line_scroll_y(120);
    row.ppu_bg_table=cyc_render_line_bg_table(120);
    row.qualified=qualified?1:0;
    if(qualified) {
        row.camera_x=state.last_camera_x;
        row.camera_y=state.last_camera_y;
        row.top=state.camera_top;
        row.tested=state.tested;
        row.matched=state.matches;
    }
    const CviiiWorldEntry5H2 *e=cviii_world_index_find_5h2(&state.world,
        row.stage,row.area,row.room);
    row.world_placed=e && e->placed;
    row.source=output;
    row.gate_5h4=state.gate;
    row.best_tested_5h4=state.best_tested;
    row.best_matched_5h4=state.best_matched;
    row.best_x_5h4=state.best_x;
    row.best_y_5h4=state.best_y;
    row.best_top_5h4=state.best_top;
    row.anchor_5h4=state.anchor;
    row.chr_resolved_5h4=state.chr_resolved;
    row.world_camera_x_5h5=state.world_camera_x;
    row.world_best_x_5h5=state.best_world_x;
    row.world_crossed_5h5=state.world_crossed;
    row.world_active_5h5=state.world_camera_active?1u:0u;
    (void)cviii_camera_trace_append_5h3(&state.trace,&row);
    if(state.ppu_trace.file){
        CviiiPpuProbe5H6 probe={ppu_scroll_x,ppu_scroll_y,ppu_bg_table,ppu_mask};
        CviiiPpuSummary5H6 pp;
        if(cviii_ppu_probe_5h6(&probe,&pp))
            (void)cviii_ppu_append_5h6(&state.ppu_trace,row.frame,row.stage,
                row.area,row.room,state.gate,row.qualified,&pp,
                state.chr_resolved?state.last_banks:NULL,state.chr_resolved!=0);
    }
}
/* Only a fully qualified single-room image may become an outgoing source.
 * Copy room metadata: state.active is overwritten on the next room. */
static void remember_verified(const CviiiRoomRenderSource *src,unsigned cx,unsigned cy){
    if(!src||!src->room)return;
    state.last_verified.room=*src->room;
    state.last_verified.source=*src;
    state.last_verified.source.room=&state.last_verified.room;
    state.last_verified.x=cx;state.last_verified.y=cy;
    state.last_verified.top=state.camera_top;
    state.last_verified.scroll_x=cyc_render_line_scroll_x(120);
    state.last_verified.scroll_y=cyc_render_line_scroll_y(120);
    state.last_verified.frame=state.frame_seq;
    state.last_verified.valid=true;
}
/* Last verified room + incoming ROM room. Both must independently agree with
 * their portion of the native framebuffer; never assert a world connection. */
static bool paint_mixed_entry(const CviiiRoomRenderSource *incoming,
                              const uint32_t *native,unsigned width){
    if(!incoming||!incoming->room||!native||!state.last_verified.valid||
       state.frame_seq>state.last_verified.frame+480u)return false;
    const CviiiWholeRoomView *old=&state.last_verified.room;
    if(old->stage==incoming->room->stage &&old->area==incoming->room->area&&
       old->room==incoming->room->room)return false;
    if(state.scene_epoch.epoch>1)return false;
    static const int offsets[]={0,-8,8};
    static const int incoming_offsets[]={0,-1,1};
    const uint8_t *oam=cyc_render_oam();
    const CviiiWholeRoomView *previous=&state.last_verified.room;
    const bool old_vertical=(previous->metadata&0x80u)!=0;
    int64_t ox=0,oy=0;
    if(!cviii_transition_step_5h8d(&state.transition_5h8d,
         state.last_verified.frame,state.frame_seq,previous->stage,
         previous->area,previous->room,(int)state.last_verified.x,
         (int)state.last_verified.y,state.last_verified.scroll_x,
         state.last_verified.scroll_y,cyc_render_line_scroll_x(120),
         cyc_render_line_scroll_y(120),old_vertical,&ox,&oy))return false;
    const int nx0=(int)state.best_x,ny0=(int)state.best_y;
    CviiiTransitionPlan5H8D best={0};
    unsigned bx=0,by=0,bnx=0,bny=0;
    for(unsigned i=0;i<sizeof offsets/sizeof offsets[0];i++){
        int64_t cx=ox+(old_vertical?0:offsets[i]);
        int64_t cy=oy+(old_vertical?offsets[i]:0);
        if(cx<0||cy<0||cx>INT32_MAX||cy>INT32_MAX)continue;
        for(unsigned j=0;j<3;j++){
            int64_t nx=nx0+(incoming->room->metadata&0x80u?0:incoming_offsets[j]);
            int64_t ny=ny0+(incoming->room->metadata&0x80u?incoming_offsets[j]:0);
            if(nx<0||ny<0||nx>INT32_MAX||ny>INT32_MAX)continue;
            CviiiTransitionPlan5H8D m=cviii_transition_best_5h8d(
                &state.last_verified.source,(unsigned)cx,(unsigned)cy,
                state.last_verified.top,incoming,(unsigned)nx,(unsigned)ny,
                state.best_top,native,oam);
            if(m.score.tested>best.score.tested ||
               (m.score.tested==best.score.tested &&
                m.score.matched>best.score.matched)){
                best=m;bx=(unsigned)cx;by=(unsigned)cy;
                bnx=(unsigned)nx;bny=(unsigned)ny;
            }
        }
    }
    if(!best.score.tested)return false;
    if(!cviii_transition_paint_5h8d(&state.last_verified.source,bx,by,
            state.last_verified.top,incoming,bnx,bny,state.best_top,
            best,state.image,width,MARGIN))return false;
    state.tested=best.score.tested;state.matches=best.score.matched;
    state.last_camera_x=bnx;state.last_camera_y=bny;
    state.camera_top=state.best_top;
    state.gate=CVIII_GATE_DUAL_PRESENTED;
    return true;
}
/* Cached observed pixels cannot imply an unseen repeating PPU page. */
static void dynamic_observe(const uint32_t *native){
    if(!native||state.scene_epoch.epoch<=1)return;
    int sx[240],sy[240];
    for(int y=0;y<240;y++){
        sx[y]=cyc_render_line_scroll_x(y);sy[y]=cyc_render_line_scroll_y(y);
    }
    cviii_boss_cache_observe_5h8c(&state.boss_cache,state.frame_seq,native,
                                  cyc_render_oam(),sx,sy);
}
static bool dynamic_paint(const uint32_t *native){
    if(!native||state.scene_epoch.epoch<=1)return false;
    int sx[240],sy[240];
    for(int y=0;y<240;y++){
        sx[y]=cyc_render_line_scroll_x(y);sy[y]=cyc_render_line_scroll_y(y);
    }
    unsigned matched=0;
    unsigned tested=cviii_boss_cache_match_stable_5h8d(&state.boss_cache,native,
                                 cyc_render_oam(),sx,sy,&matched);
    if(tested<90||(uint64_t)matched*100<(uint64_t)tested*96)return false;
    if(!cviii_boss_cache_paint_stable_5h8d(&state.boss_cache,state.image,sx,sy))return false;
    state.tested=tested;state.matches=matched;
    state.last_camera_x=0;state.last_camera_y=0;state.camera_top=0;
    state.gate=CVIII_GATE_DYNAMIC_PRESENTED;
    return true;
}
/* An independently qualified camera anchors observed background mutations.
 * PPU observations are never mistaken for offscreen enemy/item simulation. */
static bool mutation_expected(void *ctx,unsigned x,unsigned y,uint32_t *out){
    const CviiiRoomRenderSource *src=(const CviiiRoomRenderSource *)ctx;
    CviiiGfxPixel px;
    if(!cviii_room_gfx_sample(src->graphics,src->room,src->chr,
         src->bg_bank_1k,x,y,&px))return false;
    return cviii_room_gfx_color(&px,src->palette_argb,out);
}
static void mutation_overlay(const CviiiRoomRenderSource *src,
    const uint32_t *native,unsigned cx,unsigned cy,unsigned width){
    if(!src||!src->room||!native||state.mode!=CVIII_ROOM_EXTENDED)return;
    if(state.scene_event.flags&
       (CVIII_EPOCH_ROOM|CVIII_EPOCH_MAJOR_CIRAM|CVIII_EPOCH_CHR|
        CVIII_EPOCH_PALETTE|CVIII_EPOCH_DISCONTINUITY))return;
    cviii_mut_observe_5h8e(&state.mutable_5h8e,state.frame_seq,native,
        cyc_render_oam(),cx,cy,state.camera_top,mutation_expected,(void *)src);
    unsigned painted=cviii_mut_paint_5h8e(&state.mutable_5h8e,state.image,width,HEIGHT,
                           MARGIN,cx,cy,state.camera_top);
    if(state.mutations_trace && (painted||cviii_mut_count_5h8e(&state.mutable_5h8e))){
        fprintf(state.mutations_trace,"%llu,%u,%u,%u,%u,%u\n",
            (unsigned long long)state.frame_seq,src->room->stage,
            src->room->area,src->room->room,
            cviii_mut_count_5h8e(&state.mutable_5h8e),painted);
    }
}
static void erase_hud_margins(unsigned width,unsigned left,unsigned right)
{
    for (unsigned y=0;y<48u;++y) {
        uint32_t *row=state.image+(size_t)y*width;
        for (unsigned x=0;x<left;++x) row[x]=MATTE;
        for (unsigned x=right;x<width;++x) row[x]=MATTE;
    }
}
const uint32_t *cviii_room_diag_present(const uint32_t *original,int *width,int *height)
{
    if (!original || !width || !height || state.mode==CVIII_ROOM_OFF) return original;
    cviii_ref_capture_present_5h8l2(state.frame_seq, original);
    const unsigned w=state.mode==CVIII_ROOM_COMPARE?COMPARE_W:WIDE_W;
    const unsigned origin=state.mode==CVIII_ROOM_COMPARE?0u:MARGIN;
    if(state.mode==CVIII_ROOM_COMPARE) {
        /* Side-by-side is not a centered canvas: source at x=0..255. */
        for(unsigned y=0;y<HEIGHT;y++) {
            uint32_t *row=state.image+(size_t)y*w;
            memcpy(row,original+(size_t)y*NATIVE_W,NATIVE_W*sizeof(uint32_t));
            for(unsigned x=NATIVE_W;x<w;x++)row[x]=MATTE;
        }
    } else {
        cviii_room_diag_center_copy(state.image,w,HEIGHT,original,MATTE);
    }
    CviiiRoomRenderSource room;
    unsigned cx=0,cy=0;
    state.matches=state.tested=state.best_tested=state.best_matched=0;
    state.best_x=state.best_y=state.best_top=state.anchor=0;
    state.chr_resolved=0;
    state.world_camera_active=false;state.world_camera_x=0;
    state.local_active=false;
    state.world_crossed=0;state.best_world_x=0;
    state.gate=CVIII_GATE_NOT_EVALUATED;
    bool available=state.source!=SOURCE_PPU && room_source(&room,&cx,&cy);
    bool rendered=false;
    if(available && !state.local_active && state.world.loaded &&
       state.mode==CVIII_ROOM_EXTENDED &&
       (state.source==SOURCE_ATLAS || state.source==SOURCE_AUTO)) {
        rendered=state.world_camera_active &&
           cviii_world_margins_at_5h5(&state.world,&state.rooms,&room,
               state.world_camera_x,0,state.camera_top,state.image,w,HEIGHT,origin)>0;
        if(rendered) {
            state.gate=CVIII_GATE_ATLAS_PRESENTED;
            *width=(int)w;*height=(int)HEIGHT;
            mutation_overlay(&room,original,cx,cy,w);
            erase_hud_margins(w,MARGIN,MARGIN+NATIVE_W);
            remember_verified(&room,cx,cy);
            dynamic_observe(original);
            report(state.mode,OUTPUT_ATLAS);
            trace_presentation(OUTPUT_ATLAS,available);
            return state.image;
        }
        const CviiiWorldEntry5H2 *entry=cviii_world_index_find_5h2(&state.world,
           room.room->stage,room.room->area,room.room->room);
        state.gate=(entry && entry->placed)?
           CVIII_GATE_NO_MARGIN:CVIII_GATE_WORLD_UNPLACED;
    }
    if(available && state.local_active && state.mode==CVIII_ROOM_EXTENDED){
        rendered=cviii_local_margins_5h8(&room,cx,cy,state.camera_top,
                                   state.image,w,HEIGHT,origin)>0;
        if(rendered){
            state.gate=CVIII_GATE_LOCAL_PRESENTED;
            *width=(int)w;*height=(int)HEIGHT;
            mutation_overlay(&room,original,cx,cy,w);
            erase_hud_margins(w,MARGIN,MARGIN+NATIVE_W);
            remember_verified(&room,cx,cy);
            dynamic_observe(original);
            report(state.mode,OUTPUT_LOCAL);
            trace_presentation(OUTPUT_LOCAL,1);
            return state.image;
        }
        state.gate=CVIII_GATE_NO_MARGIN;
    }
    /* Only after the ordinary strict room gate fails: test two independent
     * room segments, followed by observed-only dynamic scene background. */
    if(!available && state.mode==CVIII_ROOM_EXTENDED &&
       state.source!=SOURCE_PPU && state.chr_resolved &&
       state.gate==CVIII_GATE_LOCAL_CAMERA_MISMATCH &&
       paint_mixed_entry(&room,original,w)){
        erase_hud_margins(w,MARGIN,MARGIN+NATIVE_W);
        *width=(int)w;*height=(int)HEIGHT;
        dynamic_observe(original);
        report(state.mode,OUTPUT_DUAL);
        trace_presentation(OUTPUT_DUAL,1);
        return state.image;
    }
    if(!available && state.mode==CVIII_ROOM_EXTENDED &&
       state.source!=SOURCE_PPU && dynamic_paint(original)){
        erase_hud_margins(w,MARGIN,MARGIN+NATIVE_W);
        *width=(int)w;*height=(int)HEIGHT;
        dynamic_observe(original);
        report(state.mode,OUTPUT_DYNAMIC);
        trace_presentation(OUTPUT_DYNAMIC,1);
        return state.image;
    }
    if (available && state.source==SOURCE_ATLAS && !state.world.loaded)
        state.gate=CVIII_GATE_ASSETS_MISSING;
    if (available && (state.mode==CVIII_ROOM_COMPARE ||
        state.source==SOURCE_ROOM ||
        (state.source==SOURCE_AUTO && !state.world.loaded))) {
        if (state.mode==CVIII_ROOM_COMPARE) {
            rendered=cviii_room_render_view(&room,cx,cy,state.image+NATIVE_W,
                                            NATIVE_W,HEIGHT,w);
        } else {
            /* Render every available side segment, including camera near zero. */
            rendered=true;
            if (cx>=MARGIN) {
                rendered=cviii_room_render_view(&room,cx-MARGIN,cy,
                                                state.image,MARGIN,HEIGHT,w);
            } else if (cx>0) {
                rendered=cviii_room_render_view(&room,0,cy,
                                state.image+(MARGIN-cx),cx,HEIGHT,w);
            }
            if (rendered) rendered=cviii_room_render_view(&room,cx+NATIVE_W,cy,
                                              state.image+MARGIN+NATIVE_W,
                                              MARGIN,HEIGHT,w);
        }
    }
    int output=rendered?OUTPUT_ROOM:OUTPUT_MATTE;
    if(rendered)state.gate=CVIII_GATE_ROOM_PRESENTED;
    if(available && !rendered && state.gate==CVIII_GATE_NOT_EVALUATED)
        state.gate=CVIII_GATE_NO_MARGIN;
    if(!rendered) {
        /* A partially rejected room render is never permitted to leak. */
        if(state.mode==CVIII_ROOM_COMPARE) {
            for(unsigned y=0;y<HEIGHT;y++)
                for(unsigned x=NATIVE_W;x<COMPARE_W;x++)
                    state.image[(size_t)y*COMPARE_W+x]=MATTE;
        } else {
            for(unsigned y=0;y<HEIGHT;y++) {
                for(unsigned x=0;x<MARGIN;x++)state.image[(size_t)y*WIDE_W+x]=MATTE;
                for(unsigned x=MARGIN+NATIVE_W;x<WIDE_W;x++)
                    state.image[(size_t)y*WIDE_W+x]=MATTE;
            }
        }
    }
    state.last_guard=CVIII_PPU_BAD_INPUT;
    if(!rendered && state.source==SOURCE_AUTO && !state.world.loaded) {
        CviiiPpuResult5G2 result=paint_verified_ppu(original,w,origin);
        state.last_guard=result;
        if(result==CVIII_PPU_OK) {
            output=OUTPUT_VERIFIED_PPU;state.gate=CVIII_GATE_PPU_CHECKED;
        } else if(!available)state.gate=CVIII_GATE_PPU_REJECTED;
        /* A rejected guess must remain matte; forced PPU stays explicit. */
    }
    if(!rendered && state.source==SOURCE_PPU) {
        for(unsigned y=0;y<HEIGHT;++y) {
            if(state.mode==CVIII_ROOM_COMPARE)
                paint_ppu_margin(state.image,w,y,NATIVE_W,NATIVE_W,0);
            else {
                paint_ppu_margin(state.image,w,y,0,MARGIN,-(int)MARGIN);
                paint_ppu_margin(state.image,w,y,MARGIN+NATIVE_W,MARGIN,NATIVE_W);
            }
        }
        output=OUTPUT_PPU;state.gate=CVIII_GATE_FORCED_PPU;
    }
    if (state.mode==CVIII_ROOM_EXTENDED) erase_hud_margins(w,MARGIN,MARGIN+NATIVE_W);
    /* The native framebuffer and its coordinate origin never change. */
    *width=(int)w;
    *height=(int)HEIGHT;
    dynamic_observe(original);
    report(state.mode,output);
    trace_presentation(output,available);
    return state.image;
}

/* Used by CPU/PPU/OAM evidence recorder to align frame identifiers. */
uint64_t cviii_room_diag_frame_id(void){return state.frame_seq;}

/* Called ONLY from a game routine whose stable ID and producer PC have been
 * independently verified. There are deliberately no speculative producers. */
bool cviii_room_diag_game_entity_event_5h8f(const CviiiEntityEvent5H8F *e){
    if(!e || !state.valid || !state.frame_seq)return false;
    if(!cviii_entity_emit_5h8f(&state.entities_5h8f,state.frame_seq,e))
        return false;
    if(state.entities_trace){
        fprintf(state.entities_trace,"%llu,%u,%u,%u,%u,%u,%u,%u,%u\n",
             (unsigned long long)state.frame_seq,(unsigned)e->stage,
             (unsigned)e->area,(unsigned)e->room,(unsigned)e->id,
             (unsigned)e->kind,(unsigned)e->value,(unsigned)e->scope,
             (unsigned)e->producer_pc);
        fflush(state.entities_trace);
    }
    return true;
}
