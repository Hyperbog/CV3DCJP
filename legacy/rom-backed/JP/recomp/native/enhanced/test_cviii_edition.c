/* ROM-free independent presentation/edition contract, no 6502 reconstruction. */
#include "cyc_host_extras.h"
#include "cyc_core.h"
#include "cyc_render.h"
#include "cpu6502.h"
#include "cyc_trace.h"
#include "cviii_chr_fetch_0600.h"
#include "cviii_nt_trace_0606.h"
#include "cviii_audio_menu_0620.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* JP0638: the pinned production core provides the real CPU cycle counter.
 * The isolated ROM-free host test links no guest CPU, so it supplies an
 * explicitly inert counter; this must not be linked into the game binary. */
uint64_t cyc_cycle_count(void) { return UINT64_C(0); }
/* JP0660 strict isolated host mock. Production always links the genuine core. */
Cpu6502 cpu;
void (*cyc_cpu_rts_observer)(void);
uint8_t (*cyc_cpu_ram_read_policy)(uint16_t,uint16_t,uint8_t);
bool cyc_trace_enabled;
void (*cyc_trace_cycle_hook)(const CycTraceCycle *);
unsigned hw_prg_bank4(uint16_t addr) { (void)addr; return 0; }
bool cyc_debug_peek(uint16_t addr,uint8_t *v) {
    (void)addr; (void)v; return false;
}
/* Read-only JP0599 debug interface mocks for isolated host extension test. */
const uint8_t *cyc_ppu_palette(void) {
    static uint8_t palette[32];
    return palette;
}
void cyc_hw_state_dump(void *target) {
    FILE *out = (FILE *)target;
    if (!out) return;
    fprintf(out, "cart.ctrl 20\n");
    for (unsigned i = 0; i < 8; ++i)
        fprintf(out, "cart.vrc_chr[%u] %03X\n", i, i);
    for (unsigned i = 0; i < 8; ++i)
        fprintf(out, "cart.chr_off[%u] %06X\n", i, i * 1024);
}

static CviiiChrFetchHook capture_hook;
void cviii_chr_fetch_set_hook(CviiiChrFetchHook hook) { capture_hook = hook; }
static CviiiNtHook nt_hook;
void cviii_nt_set_hook(CviiiNtHook hook) { nt_hook = hook; }
static uint32_t native[256*240];
static int capture_enabled, compose_calls, bg_reads;
const uint32_t *cyc_frame_argb(void) { return native; }
const uint8_t *cyc_cpu_ram(void) {
    static uint8_t ram[0x800];
    return ram;
}
/* JP0601 pure read-only original PPU views for the isolated host test. */
const uint16_t *cyc_frame_index(void) {
    static uint16_t pixels[256*240];
    return pixels;
}
const CycLine *cyc_frame_lines(void) {
    static CycLine lines[240];
    return lines;
}
const uint8_t *cyc_frame_bg_opaque(void) { static uint8_t mask[256*240]; return mask; }
/* Host-independent JP0608 layer accessors for headless C11 regression.
 * The independent actual layering tests exercise pixel priorities. */
static uint8_t mock_oam[256];
const uint8_t *cyc_render_oam(void) { return mock_oam; }
const uint8_t *cyc_ppu_oam(void) { return mock_oam; }
uint8_t cyc_render_chr(uint16_t addr) { (void)addr; return 0; }
uint32_t cyc_render_color(int palette) { return (uint32_t)palette; }
uint8_t cyc_render_line_mask(int y) { (void)y; return 0x14; }
bool cyc_render_line_sprite16(int y) { (void)y; return false; }
uint16_t cyc_render_line_sprite_table(int y) { (void)y; return 0; }
void cyc_render_capture_background(bool enabled) { capture_enabled=enabled; }
bool cyc_render_background(uint32_t *out) {
    bg_reads++;
    if (!capture_enabled) return false;
    for (int i=0;i<256*240;++i) out[i]=0xFF003366u;
    return true;
}
void cyc_render_sprites(uint32_t *out,int w,int h,int x,const uint8_t *bg,
                        CycSpritePlaceFn place, void *ctx) {
    (void)x;(void)place;(void)ctx;(void)bg;
    assert(w==256 && h==240);
    compose_calls++;
    out[10]=0xFF00FFFFu;
}
int main(void) {
    const CycHostExtras *e=cyc_host_extras();
    assert(e && e->options && e->option && e->present);
    /* JP0646: all diagnostic CLI options must be advertised to the host,
     * rather than silently unreachable despite an implemented callback. */
    assert(e->option_count == 23);
    const char *const required[]={
        "--sound-request-trace","--sound-decision-trace",
        "--dmc-dma-trace","--audio-session-id","--audio-epoch-trace",
        "--room-resources","--room-graphics","--room-preview",
        "--native-room-verify"
    };
    for(size_t i=0;i<sizeof required/sizeof required[0];++i) {
        size_t seen=0;
        for(size_t j=0;j<e->option_count;++j)
            if(!strcmp(e->options[j].name,required[i]))++seen;
        assert(seen==1u);
    }
    assert(!e->option(e->ctx,"--sound-decision-trace",NULL));
    assert(!e->option(e->ctx,"--dmc-dma-trace",NULL));
    assert(!cviii_audio_menu_active());
    assert(!cviii_audio_menu_toggle()); /* no authenticated original CHR */
    assert(!cviii_audio_menu_active());
    assert(!cviii_audio_menu_move(2));
    cviii_audio_menu_close();
    assert(e->audio_mix != NULL);
    assert(!e->option(e->ctx,"--asset-index",(const char *)0));
    assert(!e->option(e->ctx,"--native-room-verify",NULL));
    assert(!e->option(e->ctx,"--room-resources",NULL));
    assert(!e->option(e->ctx,"--room-graphics",NULL));
    assert(!e->option(e->ctx,"--room-preview","invalid"));
    assert(e->option(e->ctx,"--room-preview","0"));
    assert(!e->option(e->ctx,"--audio-ui-preview","nonboolean"));
    assert(e->option(e->ctx,"--audio-ui-preview","1"));
    int w=0,h=0;
    e->power_on(e->ctx);
    assert(capture_enabled);
    assert(e->present(e->ctx,&w,&h)==native && w==256 && h==240);
    /* Missing authenticated CHR must never substitute fabricated menu art. */
    assert(e->present(e->ctx,&w,&h)==native);
    assert(!compose_calls && !bg_reads);
    assert(e->option(e->ctx,"--unlimited-sprites","1"));
    assert(!e->option(e->ctx,"--stable-sprites","invalid"));
    e->power_on(e->ctx);
    assert(capture_enabled);
    assert(e->present(e->ctx,&w,&h)==native);
    assert(e->option(e->ctx,"--edition","enhanced"));
    assert(!e->option(e->ctx,"--edition","unknown"));
    e->power_on(e->ctx);
    assert(capture_enabled);
    const uint32_t *p=e->present(e->ctx,&w,&h);
    assert(p!=native && p[10]==0xFF00FFFFu);
    assert(compose_calls==1 && bg_reads==1);
    e->load_setting(e->ctx,"UnlimitedSprites","0");
    e->power_on(e->ctx);
    assert(capture_enabled && e->present(e->ctx,&w,&h)==native);
    e->load_setting(e->ctx,"Edition","Original");
    assert(e->present(e->ctx,&w,&h)==native);
    assert(e->option(e->ctx,"--master-volume","50"));
    assert(e->option(e->ctx,"--music-volume","28"));
    assert(e->option(e->ctx,"--sfx-volume","64"));
    assert(!e->option(e->ctx,"--master-volume","101"));
    assert(!e->option(e->ctx,"--music-volume","-1"));
    assert(!e->option(e->ctx,"--sfx-volume","99x"));
    int16_t pcm[]={10000,-10000,100,-100};
    /* The internal original diagnostic mode does not scale its sound. */
    e->audio_mix(e->ctx,pcm,4);
    assert(pcm[0]==10000 && pcm[1]==-10000);
    assert(e->option(e->ctx,"--edition","enhanced"));
    e->audio_mix(e->ctx,pcm,4);
    assert(pcm[0]==5000 && pcm[1]==-5000 && pcm[2]==50 && pcm[3]==-50);
    assert(e->option(e->ctx,"--edition","original"));
    e->load_setting(e->ctx,"MasterVolume","1");
    assert(e->option(e->ctx,"--edition","enhanced"));
    int16_t one[]={10000};
    e->audio_mix(e->ctx,one,1);
    assert(one[0]==100);
    assert(e->option(e->ctx,"--edition","original"));
    e->load_setting(e->ctx,"MasterVolume","50");
    FILE *f=tmpfile();assert(f);e->save_settings(e->ctx,f);rewind(f);
    char text[256]={0};size_t n=fread(text,1,sizeof(text)-1,f);assert(n>0);fclose(f);
    assert(strstr(text,"Edition = Original") && strstr(text,"UnlimitedSprites = 0"));
    assert(strstr(text,"MasterVolume = 50"));
    assert(strstr(text,"MusicVolume = 28"));
    assert(strstr(text,"SoundEffectsVolume = 64"));
    puts("PASS CVIII Original fallback / Enhanced compositor / persisted settings");
    return 0;
}
