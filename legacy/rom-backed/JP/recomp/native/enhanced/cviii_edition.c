/* CVIII edition presentation, JP 0589.
 * Written for the pinned external NESRecomp cyc_host_extras ABI.
 * Game execution, CPU-visible PPU flags, mapper banking and timing are untouched.
 * No ROM data or NESRecomp source is copied here.
 */
#include "cyc_host_extras.h"
#include "cyc_core.h"
#include "cyc_render.h"
#include "cviii_chr_index_0598.h"
#include "cviii_whole_room.h"
#include "cviii_room_gfx.h"
#include "cviii_room_preview.h"
#include "cviii_room_native_host_0660.h"
#include "cviii_chr_audit_0599.h"
#include "cviii_chr_fetch_0600.h"
#include "cviii_frame_evidence_0601.h"
#include "cviii_nt_trace_0606.h"
#include "cviii_sprite_layers_0608.h"
#include "cviii_object_oam_trace_0609.h"
#include "cviii_audio_bus_0610.h"
#include "cviii_sound_requests_0612.h"
#include "cviii_sound_decisions_0646.h"
#include "cviii_dmc_dma_trace_0650.h"
#include "cviii_audio_session_0614.h"
#include "cviii_audio_levels_0618.h"
#include "cviii_virtual_audio_0647.h"
#include "cviii_audio_ui_0619.h"
#include "cviii_menu_registry_0620.h"
#include "cviii_audio_menu_0620.h"
#include "cviii_audio_menu_state_0622.h"
#include "cviii_start_menu_0623.h"
#include "cviii_start_host_0623.h"
#include "cviii_start_ui_0623.h"
#include "cviii_slot_picker_0625.h"
#include "cviii_menu_stack_0626.h"
#ifdef CYC_WITH_RECOMP_UI
#include "recomp_runtime_ui.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CVIII_W 256
#define CVIII_H 240

typedef struct {
    int enhanced;
    int sprite_unlimited;
    int stable_sprites; /* optional host-only temporal presentation; off by default */
    CviiiSpriteLayers sprite_layers;
    CviiiObjectOamTrace object_trace; /* read-only JP PRG26 object/OAM correlation */
    CviiiAudioBus audio_bus; /* opt-in nonmutating 2A03/VRC6 sound writes */
    CviiiSoundRequests sound_requests; /* original fixed $E249 ingress only */
    CviiiSoundDecisions sound_decisions; /* opt-in $0C sound terminal observations */
    CviiiDmcDmaTrace dmc_dma_trace; /* opt-in actual DMC fetch physical-bank evidence */
    CviiiAudioSession audio_session; /* extra-CPU row-count epoch markers */
    CviiiAudioLevels volume; /* enhanced host gains, never emulated RAM */
    CviiiVirtualAudio virtual_audio; /* dormant until original program decoder is verified */
    int audio_ui_preview; /* opt-in independent visual QA */
    CviiiAudioMenuState audio_ui; /* F6 modal selection, no guest RAM */
    CviiiStartMenu start_menu; /* F4 menu actions, no guest RAM */
    CviiiSlotPicker slot_menu; /* JP0625, host-selected save file number */
    char audio_session_id[33]; /* launcher-supplied private session GUID */
    uint8_t layer_masks[CVIII_LAYER_H];
    CviiiChrIndex assets; /* read-only imported physical CHR; presentation only */
    CviiiWholeRoomPack rooms; /* immutable owner-local preloaded room resources */
    CviiiRoomGfx gfx; /* native pattern quads and palette attributes */
    CviiiRoomNativeBridge0660 room_native_bridge; /* original 6502 execution observer only */
    const CviiiGfxGroup *active_tileset;
    int room_preview; /* opt-in; original game remains unchanged */
    uint32_t room_preview_pixels[CVIII_ROOM_PREVIEW_W * CVIII_ROOM_PREVIEW_H];
    CviiiWholeRoomView active_room;
    int active_room_valid;
    CviiiChrAudit audit; /* opt-in read-only per-frame bank audit */
    CviiiChrFetchAudit fetch; /* opt-in actual PPU pattern-read observation */
    CviiiFrameEvidence frame_evidence; /* opt-in correlated PPU evidence */
    CviiiNtTrace nt_trace; /* opt-in real nametable and attribute reads */
    uint32_t presentation[CVIII_W * CVIII_H];
} CviiiEdition;

static CviiiEdition cviii = { .volume = CVIII_AUDIO_LEVELS_DEFAULT };
static int cviii_start_ready;

/* JP0620: optional built-in paused runtime menu rows for recomp-ui builds.
 * The separate F6 modal now uses the original CHR overlay on all supported
 * private windowed builds, including those built without recomp-ui.
 * Native pause/input routing remains in the separately pinned SDL overlay. */
#ifdef CYC_WITH_RECOMP_UI
typedef struct {
    CviiiAudioLevels *levels;
    CviiiVolumeChannel channel;
} CviiiVolumeMenuBinding;
static CviiiMenuRegistry cviii_menu;
static CviiiVolumeMenuBinding cviii_menu_bindings[3] = {
    {&cviii.volume,CVIII_VOLUME_MASTER},
    {&cviii.volume,CVIII_VOLUME_MUSIC},
    {&cviii.volume,CVIII_VOLUME_SFX}
};
static RecompRuntimeUiItem cviii_ui_rows[CVIII_MENU_MAX_ITEMS];
static RecompRuntimeUiCallbacks cviii_ui_callbacks;
static int cviii_menu_ready;

static int cviii_menu_get_volume(void *user,int *value)
{
    CviiiVolumeMenuBinding *b=(CviiiVolumeMenuBinding *)user;
    *value=cviii_audio_levels_get(b->levels,b->channel);
    return *value>=0;
}
static int cviii_menu_set_volume(void *user,int value)
{
    CviiiVolumeMenuBinding *b=(CviiiVolumeMenuBinding *)user;
    return cviii_audio_levels_set(b->levels,b->channel,value);
}
static int cviii_host_menu_get(void *ctx,const RecompRuntimeUiItem *it,int *out)
{
    return cviii_menu_get((const CviiiMenuRegistry *)ctx,it->key,out);
}
static int cviii_host_menu_set(void *ctx,const RecompRuntimeUiItem *it,int v)
{
    return cviii_menu_set((const CviiiMenuRegistry *)ctx,it->key,v);
}
static void cviii_init_audio_menu(CycHostExtras *extras)
{
    if(cviii_menu_ready)return;
    static const char *const keys[3]={
        "cviii.audio.master","cviii.audio.music","cviii.audio.sfx"
    };
    static const char *const labels[3]={
        "Master Volume","Music Volume (pending mixer)",
        "Sound Effects Volume (pending mixer)"
    };
    static const char *const descriptions[3]={
        "Enhanced output PCM master level, 0 to 100 in one-percent steps.",
        "Saved preference only until independent music voice mixing exists.",
        "Saved preference only until independent sound-effect mixing exists."
    };
    for(unsigned i=0;i<3u;++i) {
        CviiiMenuSetting setting={
            keys[i],"Audio",labels[i],descriptions[i],
            0,100,1,&cviii_menu_bindings[i],
            cviii_menu_get_volume,cviii_menu_set_volume
        };
        if(!cviii_menu_register(&cviii_menu,&setting)){
            fprintf(stderr,"CVIII JP0620: audio menu registration failed\n");
            return;
        }
    }
    /* Additional game-owned settings can be registered above without any
     * change to the host's navigation, save or pause behavior. */
    for(size_t i=0;i<cviii_menu.count;++i) {
        const CviiiMenuSetting *item=&cviii_menu.items[i];
        cviii_ui_rows[i]=(RecompRuntimeUiItem){
            .key=item->key, .section=item->section,
            .label=item->label, .description=item->description,
            .type=RECOMP_RUNTIME_UI_INT, .minimum=item->minimum,
            .maximum=item->maximum, .step=item->step
        };
    }
    cviii_menu_seal(&cviii_menu);
    cviii_ui_callbacks=(RecompRuntimeUiCallbacks){
        .context=&cviii_menu,
        .get_value=cviii_host_menu_get,
        .set_value=cviii_host_menu_set
    };
    extras->menu_items=cviii_ui_rows;
    extras->menu_item_count=cviii_menu.count;
    extras->menu_callbacks=&cviii_ui_callbacks;
    cviii_menu_ready=1;
}
#endif

static int parse_boolean(const char *text, int *value)
{
    if (!strcmp(text, "1") || !strcmp(text, "on") || !strcmp(text, "true")) {
        *value = 1;
        return 1;
    }
    if (!strcmp(text, "0") || !strcmp(text, "off") || !strcmp(text, "false")) {
        *value = 0;
        return 1;
    }
    return 0;
}

static int set_edition(const char *name)
{
    if (!strcmp(name, "original") || !strcmp(name, "Original")) {
        cviii_audio_menu_state_close(&cviii.audio_ui);
        cviii_start_close(&cviii.start_menu);
        cviii_slot_cancel(&cviii.slot_menu);
        cviii.enhanced = 0;
        return 1;
    }
    if (!strcmp(name, "enhanced") || !strcmp(name, "Enhanced")) {
        cviii.enhanced = 1;
        return 1;
    }
    return 0;
}

static void load_setting(void *context, const char *key, const char *value)
{
    (void)context;
    if (!strcmp(key, "Edition")) (void)set_edition(value);
    else if (!strcmp(key, "UnlimitedSprites"))
        (void)parse_boolean(value, &cviii.sprite_unlimited);
    else if (!strcmp(key, "StableSprites"))
        (void)parse_boolean(value, &cviii.stable_sprites);
    else (void)cviii_audio_levels_load(&cviii.volume,key,value);
}

static void save_settings(void *context, FILE *file)
{
    (void)context;
    fprintf(file, "Edition = %s\nUnlimitedSprites = %d\nStableSprites = %d\n",
            cviii.enhanced ? "Enhanced" : "Original",
            cviii.sprite_unlimited, cviii.stable_sprites);
    cviii_audio_levels_save(&cviii.volume,file);
}

static bool option(void *context, const char *name, const char *value)
{
    (void)context;
    if (!strcmp(name, "--edition")) return value && set_edition(value);
    if (!strcmp(name, "--unlimited-sprites"))
        return value && parse_boolean(value, &cviii.sprite_unlimited);
    if (!strcmp(name, "--stable-sprites"))
        return value && parse_boolean(value, &cviii.stable_sprites);
    /* Developer overrides only; final interface will use HUD-art sliders.
     * Music/SFX levels persist but do not yet affect the unsplit PCM bus. */
    if (!strcmp(name,"--audio-ui-preview"))
        return value && parse_boolean(value,&cviii.audio_ui_preview);
    if (!strcmp(name,"--master-volume") ||
        !strcmp(name,"--music-volume") ||
        !strcmp(name,"--sfx-volume")) {
        uint8_t percent;
        if (!cviii_audio_levels_parse(value,&percent)) return false;
        CviiiVolumeChannel channel =
            !strcmp(name,"--master-volume") ? CVIII_VOLUME_MASTER :
            !strcmp(name,"--music-volume") ? CVIII_VOLUME_MUSIC : CVIII_VOLUME_SFX;
        return cviii_audio_levels_set(&cviii.volume,channel,(int)percent)!=0;
    }
    if (!strcmp(name, "--audio-reg-trace")) {
        if (!value || !cviii_audio_bus_open(&cviii.audio_bus, value)) {
            fprintf(stderr, "CVIII: cannot open private 2A03/VRC6 audio register log\n");
            return false;
        }
        printf("CVIII: tracing sound chip writes; no changes to music or effects\n");
        return true;
    }
    if (!strcmp(name, "--sound-request-trace")) {
        if (!value || !cviii_sound_requests_open(&cviii.sound_requests, value)) {
            fprintf(stderr, "CVIII: cannot open private original JP sound-request CSV\n");
            return false;
        }
        printf("CVIII: read-only original $E249 request entries; no sound changes\n");
        return true;
    }
    if (!strcmp(name, "--sound-decision-trace")) {
        if (!value || !cviii_sound_decisions_open(&cviii.sound_decisions, value)) {
            fprintf(stderr, "CVIII: cannot open private original sound-decision CSV\n");
            return false;
        }
        printf("CVIII: original sound branch observation; no audio changes\n");
        return true;
    }
    if (!strcmp(name, "--dmc-dma-trace")) {
        if (!value || !cviii_dmc_dma_trace_open(&cviii.dmc_dma_trace,value)) {
            fprintf(stderr,"CVIII: cannot open private DMC DMA physical-bank CSV\n");
            return false;
        }
        printf("CVIII: actual DMC sample fetch and mapper write intent; original audio unchanged\n");
        return true;
    }
    if (!strcmp(name, "--audio-session-id")) {
        if (!value || strlen(value)!=32 || strspn(value,"0123456789abcdef")!=32)
            return false;
        memcpy(cviii.audio_session_id,value,33);
        return true;
    }
    if (!strcmp(name, "--audio-epoch-trace")) {
        if (!value || !cviii_audio_session_open(&cviii.audio_session,
                                               value,cviii.audio_session_id)) {
            fprintf(stderr,"CVIII: private audio epoch manifest open failed\n");
            return false;
        }
        return true;
    }
    if (!strcmp(name, "--sprite-object-trace")) {
        if (!value || !cviii_object_oam_trace_open(&cviii.object_trace, value)) {
            fprintf(stderr, "CVIII: cannot open owner-local object/OAM trace output\n");
            return false;
        }
        printf("CVIII: recording actual game-object and hardware OAM per frame\n");
        return true;
    }
    if (!strcmp(name, "--asset-index")) {
        if (!value || !cviii_chr_index_load(&cviii.assets, value)) {
            fprintf(stderr, "CVIII: rejected missing, mismatched or damaged JP asset index\n");
            return false;
        }
        printf("CVIII: loaded authenticated read-only JP CHR index (8192 tiles); PPU remains authoritative\n");
        return true;
    }
    if (!strcmp(name, "--room-resources")) {
        if (!value || !cviii_whole_room_open(&cviii.rooms, value)) {
            fprintf(stderr,"CVIII: invalid owner-local JP room pack\n");
            return false;
        }
        cviii.active_room_valid=0;
        return true;
    }
    if (!strcmp(name, "--room-preview"))
        return value && parse_boolean(value,&cviii.room_preview);
    if (!strcmp(name, "--native-room-verify")) {
        if (!value || !cviii_bridge0660_load(&cviii.room_native_bridge,value) ||
            !cviii_bridge0660_attach(&cviii.room_native_bridge)) {
            fprintf(stderr,"CVIII: refused unauthenticated native-room verification pack or occupied CPU observer\n");
            return false;
        }
        return true;
    }
    if (!strcmp(name, "--room-graphics")) {
        if (!value || !cviii_room_gfx_open(&cviii.gfx,value)) {
            fprintf(stderr,"CVIII: invalid Japanese metatile pack\n");
            return false;
        }
        cviii.active_tileset=NULL;
        return true;
    }
    if (!strcmp(name, "--chr-audit")) {
        if (!value || !cviii_chr_audit_open(&cviii.audit, value)) {
            fprintf(stderr, "CVIII: cannot open requested CHR audit output\n");
            return false;
        }
        printf("CVIII: recording 1-in-60-frame read-only VRC6 bank/palette samples\n");
        return true;
    }
    if (!strcmp(name, "--chr-fetch-audit")) {
        if (!value || !cviii_chr_fetch_open(&cviii.fetch, value)) {
            fprintf(stderr, "CVIII: cannot open requested CHR fetch audit output\n");
            return false;
        }
        printf("CVIII: recording sampled actual VRC6 PPU CHR reads (no substitution)\n");
        return true;
    }
    if (!strcmp(name, "--frame-evidence")) {
        if (!value || !cviii_frame_evidence_open(&cviii.frame_evidence, value)) {
            fprintf(stderr, "CVIII: cannot open read-only PPU frame evidence output\n");
            return false;
        }
        printf("CVIII: recording sampled original PPU frame fingerprints\n");
        return true;
    }
    if (!strcmp(name, "--nt-fetch-audit")) {
        if (!value || !cviii_nt_open(&cviii.nt_trace, value)) {
            fprintf(stderr, "CVIII: cannot open PPU background tile and attribute audit\n");
            return false;
        }
        printf("CVIII: sampling true PPU nametable/attribute reads, no rendering changes\n");
        return true;
    }
    return false;
}

static void power_on(void *context)
{
    CviiiEdition *settings = (CviiiEdition *)context;
    settings->active_room_valid=0;
    settings->active_tileset=NULL;
    cviii_bridge0660_boundary(&settings->room_native_bridge);
    /* Both editions must serialize the SAME optional presentation-state
     * schema. NESRecomp registers the 'cyc.background' savestate hook ONLY
     * while capture is enabled. Disabling it in Original makes Original
     * snapshots incompatible with Enhanced snapshots, even though CPU/PPU
     * machine state is otherwise identical.
     *
     * Always capture the presentation-only background history so load-state
     * validation sees a stable auxiliary schema. Original still returns
     * untouched hardware pixels in present(), without sprite recomposition.
     * The capture does NOT affect sprite-0, overflow, IRQ, DMA or CPU state.
     * Do not recapture in frame_begin: that clears the saved history.
     */
    cviii_layers_reset(&settings->sprite_layers);
    cviii_audio_menu_state_close(&settings->audio_ui); /* reset cannot leave a stale modal */
    cviii_start_close(&settings->start_menu);
    cviii_slot_cancel(&settings->slot_menu);
    cviii_virtual_audio_clear(&settings->virtual_audio); /* no host voice survives power */
    cyc_render_capture_background(true);
    cviii_sound_decisions_boundary("power_on",cyc_cycle_count());
    cviii_dmc_dma_trace_boundary("power_on",cyc_cycle_count());
    cviii_audio_session_mark(&settings->audio_session,"power_on",
        cyc_cycle_count(),settings->sound_requests.requests,settings->audio_bus.writes,
        settings->sound_requests.output,settings->audio_bus.output);
}

static uint8_t layer_read_chr(void *user, uint16_t address)
{
    (void)user;
    return cyc_render_chr(address);
}
static uint32_t layer_color(void *user, unsigned palette_index)
{
    (void)user;
    return cyc_render_color((int)palette_index);
}
static const CviiiLayerFns layer_functions = {layer_read_chr, layer_color, NULL};

static void state_loaded(void *context)
{
    CviiiEdition *settings = (CviiiEdition *)context;
    settings->active_room_valid=0;
    settings->active_tileset=NULL;
    cviii_bridge0660_boundary(&settings->room_native_bridge);
    cviii_layers_reset(&settings->sprite_layers);
    cviii_audio_menu_state_close(&settings->audio_ui); /* loaded gameplay never owns a stale modal */
    cviii_start_close(&settings->start_menu);
    cviii_slot_cancel(&settings->slot_menu);
    cviii_virtual_audio_clear(&settings->virtual_audio); /* clear on host load epoch */
    cviii_sound_decisions_boundary("state_loaded",cyc_cycle_count());
    cviii_dmc_dma_trace_boundary("state_loaded",cyc_cycle_count());
    cviii_audio_session_mark(&settings->audio_session,"state_loaded",
        cyc_cycle_count(),settings->sound_requests.requests,settings->audio_bus.writes,
        settings->sound_requests.output,settings->audio_bus.output);
}

static void frame_begin(void *context)
{
    CviiiEdition *settings = (CviiiEdition *)context;
    if (settings->enhanced) {
        cviii_chr_fetch_frame_begin(&settings->fetch, &settings->assets);
        cviii_nt_begin(&settings->nt_trace, settings->fetch.active != 0,
                       settings->fetch.frames);
    }
}

static void frame_end(void *context)
{
    CviiiEdition *settings = (CviiiEdition *)context;
    /* Read-only lookup; original Japanese RAM selects the active room. */
    settings->active_room_valid=0;
    if (settings->rooms.allocation) {
        const uint8_t *ram=cyc_cpu_ram();
        if (ram) settings->active_room_valid=cviii_whole_room_find(
            &settings->rooms,ram[0x34],ram[0x35],ram[0x36],
            &settings->active_room);
    }
    settings->active_tileset=settings->active_room_valid ?
        cviii_room_gfx_group(&settings->gfx,&settings->active_room) : NULL;
    if (settings->object_trace.out)
        cviii_object_oam_trace_frame(&settings->object_trace,
                                    cyc_cpu_ram(), cyc_ppu_oam());
    if (settings->enhanced && settings->stable_sprites) {
        uint8_t tall[CVIII_LAYER_H];
        uint16_t tables[CVIII_LAYER_H];
        for (unsigned y=0;y<CVIII_LAYER_H;++y) {
            settings->layer_masks[y] = cyc_render_line_mask((int)y);
            tall[y] = (uint8_t)cyc_render_line_sprite16((int)y);
            tables[y] = cyc_render_line_sprite_table((int)y);
        }
        const uint8_t *oam = cyc_render_oam();
        /* $22 is the ORIGINAL game OAM emission phase. Unlike raw OAM
         * slot priority it is stable when converted back into logical
         * object emission order (stride $C4, frame shift $44). */
        const uint8_t *ram = cyc_cpu_ram();
        if (oam && ram) cviii_layers_capture(&settings->sprite_layers,
            oam, ram[0x22], settings->layer_masks, tall, tables,
            &layer_functions);
    }
    if (settings->frame_evidence.output && settings->fetch.active) {
        const CycLine *source_lines = cyc_frame_lines();
        if (source_lines) {
            uint8_t serialized_lines[CVIII_FRAME_LINES_BYTES];
            for (unsigned y = 0; y < CVIII_FRAME_HEIGHT; ++y) {
                const CycLine *line = &source_lines[y];
                serialized_lines[y * 5u] = (uint8_t)line->v;
                serialized_lines[y * 5u + 1u] = (uint8_t)(line->v >> 8);
                serialized_lines[y * 5u + 2u] = line->fine_x;
                serialized_lines[y * 5u + 3u] = line->ctrl;
                serialized_lines[y * 5u + 4u] = line->mask;
            }
            (void)cviii_frame_evidence_sample(&settings->frame_evidence,
                &settings->fetch, cyc_frame_index(),
                cyc_frame_bg_opaque(), serialized_lines);
        } else {
            ++settings->frame_evidence.input_failures;
        }
    }
    cviii_nt_end(&settings->nt_trace);
    cviii_chr_fetch_frame_end(&settings->fetch);
    if (!settings->audit.out || !settings->assets.raw || !settings->assets.pixels)
        return;
    ++settings->audit.frames;
    if ((settings->audit.frames - 1) % settings->audit.interval != 0) return;
    /* This is a read-only snapshot of the mapper and palette AFTER frame
     * execution. Mid-scanline changes are not observable at this cadence.
     * NESRecomp's existing public state-dump API avoids including any
     * internal mapper structures in our game-owned host source. */
    FILE *snapshot = tmpfile();
    if (!snapshot) {
        fprintf(stderr, "CVIII CHR audit: temporary mapper snapshot unavailable\n");
        ++settings->audit.mismatch;
        return;
    }
    cyc_hw_state_dump(snapshot);
    cviii_chr_audit_frame(&settings->audit, &settings->assets,
                          snapshot, cyc_ppu_palette());
    fclose(snapshot);
}

static const uint32_t *present_game(void *context, int *width, int *height)
{
    CviiiEdition *settings = (CviiiEdition *)context;
    *width = CVIII_W;
    *height = CVIII_H;
    if (!settings->enhanced ||
        (!settings->sprite_unlimited && !settings->stable_sprites))
        return cyc_frame_argb();
    if (!cyc_render_background(settings->presentation))
        return cyc_frame_argb();
    if (settings->stable_sprites && settings->sprite_layers.primed) {
        cviii_layers_draw(&settings->sprite_layers, settings->presentation,
                          cyc_frame_bg_opaque(), settings->layer_masks,
                          &layer_functions);
        return settings->presentation;
    }
    /* Composite up to 64 *OAM-present* sprites over the PPU background.
     * This cannot restore sprites the game never put in OAM.
     * Actual mid-frame CHR switching remains a limitation: experimental.
     * No emulated sprite overflow, sprite-0 hit, IRQ or DMA is altered.
     */
    cyc_render_sprites(settings->presentation, CVIII_W, CVIII_H, 0,
                       cyc_frame_bg_opaque(), NULL, NULL);
    return settings->presentation;
}

/* JP0623 game-owned Start/Pause state and stable action identities. */
int cviii_pause_active(void)
{
    return cviii_menu_stack_parent_active(&cviii.start_menu,&cviii.audio_ui);
}
int cviii_pause_toggle(void)
{
    if(!cviii_start_ready) {
        if(!cviii_start_defaults(&cviii.start_menu))return 0;
        cviii_start_seal(&cviii.start_menu);
        cviii_start_ready=1;
    }
    if(cviii.audio_ui.open)return 0; /* audio owns input until Back */
    if(cviii.start_menu.open)cviii_slot_cancel(&cviii.slot_menu);
    return cviii_start_toggle(&cviii.start_menu,cviii.enhanced,
                               cviii.assets.pixels!=NULL);
}
void cviii_pause_close(void)
{
    cviii_start_close(&cviii.start_menu);
    cviii_slot_cancel(&cviii.slot_menu);
}
/* JP0625: game-only slot edits; host owns authenticated state serialization. */
int cviii_pause_slot_active(void)
{
    return cviii_pause_active() && cviii_slot_active(&cviii.slot_menu);
}
int cviii_pause_slot_begin(int action,unsigned current_slot)
{
    if(!cviii_pause_active() || !cviii.enhanced)return 0;
    return cviii_slot_begin(&cviii.slot_menu,(CviiiStartAction)action,current_slot);
}
void cviii_pause_slot_cancel(void)
{
    cviii_slot_cancel(&cviii.slot_menu);
}
int cviii_pause_slot_step(int direction)
{
    return cviii_pause_active() &&
           cviii_slot_step(&cviii.slot_menu,direction);
}
int cviii_pause_slot_accept(unsigned *slot)
{
    if(!cviii_pause_active())return (int)CVIII_START_NONE;
    return (int)cviii_slot_accept(&cviii.slot_menu,slot);
}
void cviii_pause_move(int direction)
{
    if(cviii_pause_active())
        (void)cviii_start_move(&cviii.start_menu,direction);
}
int cviii_pause_accept(void)
{
    return cviii_pause_active() ?
        (int)cviii_start_accept(&cviii.start_menu) : (int)CVIII_START_NONE;
}
/* JP0626: child Audio holds parent selection, returns there after Back. */
int cviii_pause_audio_enter(void)
{
    return cviii_menu_stack_enter_audio(&cviii.start_menu,&cviii.audio_ui,
                                        cviii.enhanced,cviii.assets.pixels!=NULL);
}

/* JP0620 game-owned UI control exported to the private window host overlay.
 * No SDL dependency here; the host handles pause, input and saving config. */
int cviii_audio_menu_active(void)
{
    return cviii.audio_ui.open;
}
int cviii_audio_menu_toggle(void)
{
    return cviii_audio_menu_state_toggle(&cviii.audio_ui,
        cviii.enhanced,cviii.assets.pixels!=NULL);
}
void cviii_audio_menu_close(void)
{
    cviii_audio_menu_state_close(&cviii.audio_ui);
}
int cviii_audio_menu_move(int direction)
{
    return cviii_audio_menu_state_move(&cviii.audio_ui,&cviii.volume,direction);
}

/* Audio hook only changes presentation PCM, not APU registers or CPU state.
 * Legacy original debug mode is kept unscaled for behavioral A/B checks. */
/* JP0619: developer preview composes owner-verified CHR pixels at the
 * presentation boundary only, after the unaltered gameplay picture. */
static const uint32_t *present(void *context,int *width,int *height)
{
    CviiiEdition *settings=(CviiiEdition *)context;
    const uint32_t *frame=present_game(context,width,height);
    if (settings->enhanced && !settings->audio_ui.open &&
        settings->start_menu.open && settings->assets.pixels && *width==CVIII_W && *height==CVIII_H) {
        if(frame!=settings->presentation)
            memcpy(settings->presentation,frame,sizeof settings->presentation);
        if(cviii_pause_slot_active() &&
           cviii_start_slot_ui_draw(settings->presentation,CVIII_W,CVIII_H,
                                    &settings->assets,&settings->slot_menu))
            return settings->presentation;
        if(cviii_start_ui_draw(settings->presentation,CVIII_W,CVIII_H,
                               &settings->assets,&settings->start_menu))
            return settings->presentation;
    }
    if (!settings->enhanced ||
        (!settings->audio_ui_preview && !settings->audio_ui.open) ||
        !settings->assets.pixels || *width!=CVIII_W || *height!=CVIII_H)
        return frame;
    if (frame!=settings->presentation)
        memcpy(settings->presentation,frame,sizeof settings->presentation);
    if (!cviii_audio_ui_render_mode(settings->presentation,CVIII_W,CVIII_H,
                                    &settings->assets,&settings->volume,
                                    settings->audio_ui.open ? settings->audio_ui.selected : 0u,
                                    settings->audio_ui.open!=0))
        return frame;
    return settings->presentation;
}
static const uint32_t *present_room_comparison(void *context,int *width,int *height)
{
    CviiiEdition *settings=(CviiiEdition *)context;
    const uint32_t *original=present(context,width,height);
    if(!settings->enhanced || !settings->room_preview || !original ||
       *width!=256 || *height!=240 || !settings->active_room_valid ||
       !settings->active_tileset || !settings->assets.pixels)
        return original;
    CviiiRoomRenderSource source={0};
    source.graphics=&settings->gfx;
    source.room=&settings->active_room;
    source.chr=&settings->assets;
    source.bg_bank_1k[0]=0x40; /* room initialization defaults only */
    source.bg_bank_1k[1]=settings->active_room.bg_r5;
    source.bg_bank_1k[2]=settings->active_room.bg_r6;
    source.bg_bank_1k[3]=0x43;
    for(unsigned i=0;i<16u;i++) source.palette_argb[i]=cyc_render_color((int)i);
    unsigned camera=0;
    const uint8_t *ram=cyc_cpu_ram();
    if(ram) camera=((unsigned)ram[0x57]<<8)|ram[0x56];
    unsigned camera_x=(settings->active_room.metadata&0x80u)?0u:camera;
    unsigned camera_y=(settings->active_room.metadata&0x80u)?camera:0u;
    if(!cviii_room_preview_compose(&source,original,camera_x,camera_y,
                                   settings->room_preview_pixels))
        return original;
    *width=(int)CVIII_ROOM_PREVIEW_W;
    *height=(int)CVIII_ROOM_PREVIEW_H;
    return settings->room_preview_pixels;
}

static void audio_mix(void *context,int16_t *samples,size_t count)
{
    const CviiiEdition *settings=(const CviiiEdition *)context;
    if (settings->enhanced)
        cviii_audio_levels_master_pcm(&settings->volume,samples,count);
}

static const CycHostOption cviii_options[] = {
    {"--edition", true, "Original or Enhanced PC edition (Original default)"},
    {"--unlimited-sprites", true,
     "Enhanced experimental 64-OAM sprite compositor (0/1, default 0)"},
    {"--stable-sprites", true,
     "EXPERIMENTAL logical sprite-object layer order with 1-frame history (0/1, default 0)"},
    {"--sprite-object-trace", true,
     "Developer owner-local per-frame 28 object fields and 64 hardware OAM entries"},
    {"--audio-ui-preview", true, "Developer only: draw noninteractive authentic health-bar UI"},
    {"--master-volume", true, "Enhanced master PCM gain, 0..100 (1% increments)"},
    {"--music-volume", true, "Reserved persistent music gain, requires separate bus"},
    {"--sfx-volume", true, "Reserved persistent SFX gain, requires separate bus"},
    {"--audio-reg-trace", true,
     "Read-only 2A03/VRC6 audio register traffic for source separation research"},
    {"--sound-request-trace", true,
     "Original JP fixed $E249 sound request entry register/stack snapshots"},
    {"--sound-decision-trace", true,
     "Owner-only original PRG0C sound request/terminal decision CSV (no audio change)"},
    {"--dmc-dma-trace", true,
     "Private actual DMC DMA ROM-byte/PRG-bank fetch and VRC6 bank-write-intent CSV"},
    {"--audio-session-id", true, "Owner-local GUID binding both audio traces"},
    {"--audio-epoch-trace", true, "Owner-local power-on/load epoch row boundaries"},
    {"--asset-index", true,
     "Private read-only 655368-byte JP CHR index (no renderer substitution)"},
    {"--room-resources", true, "Private Japanese whole-room data pack"},
    {"--room-graphics", true, "Private Japanese metatile/attribute pack"},
    {"--native-room-verify", true, "Opt-in private original $0A:$B7F7 native postcondition observer"},
    {"--room-preview", true, "Experimental original/ROM-derived whole-room comparison (0/1)"},
    {"--chr-audit", true,
     "Developer CSV for sampled VRC6 CHR pages and palette (requires asset index)"},
    {"--chr-fetch-audit", true,
     "Read-only actual PPU CHR fetch vs cached ROM scanline CSV (requires asset index)"},
    {"--frame-evidence", true,
     "Original PPU 9-bit frame fingerprint CSV for same sampled fetch frames"},
    {"--nt-fetch-audit", true,
     "Read-only PPU nametable and attribute bus values for sampled frames"},
};

static CycHostExtras cviii_extra = {
    .ctx = &cviii,
    .present = present_room_comparison,
    .load_setting = load_setting,
    .save_settings = save_settings,
    .power_on = power_on,
    .frame_begin = frame_begin,
    .frame_end = frame_end,
    .state_loaded = state_loaded,
    .audio_mix = audio_mix,
    .options = cviii_options,
    .option_count = sizeof(cviii_options) / sizeof(cviii_options[0]),
    .option = option,
};

const CycHostExtras *cyc_host_extras(void)
{
#ifdef CYC_WITH_RECOMP_UI
    cviii_init_audio_menu(&cviii_extra);
#endif
    return &cviii_extra;
}
