/* Phase 5E: Original-vs-Enhanced presentation using supported cycle APIs.
 * Reuse original JP0618 audio and JP0608/0645 sprite code unchanged.
 * The PPU/mapper/CPU/gameplay state is never modified by these effects.
 */
#include "cyc_host_extras.h"
#ifdef CVIII_COLLISION_V11_SHADOW
#include "CollisionDispatchV11.h"
#endif
#include "cyc_core.h"
#include "cyc_render.h"
#include "cviii_sprite_layers_0608.h"
#include "cviii_audio_levels_0618.h"
#include "cviii_room_diag.h"
#ifdef CVIII_SELECTOR_TRACE_5H8S
#include "cviii_selector_native_5h8s.h"
#include "cviii_selector_native_5h8u.h" /* CVIII-5H8U */
#endif /* CVIII-5H8S */
#include "cviii_diagnostic_5h8.h"
#ifdef CYC_WITH_RECOMP_UI
#include "recomp_runtime_ui.h"
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int enhanced;
    CviiiAudioLevels volume;
    int unlimited_sprites;
    int stable_sprites;
    CviiiSpriteLayers layers;
    uint8_t masks[CVIII_LAYER_H];
    uint32_t presentation[CVIII_LAYER_W * CVIII_LAYER_H];
} CviiiModernHost;

static CviiiModernHost host = {.volume = CVIII_AUDIO_LEVELS_DEFAULT};

static bool cviii_boolean(const char *value, int *out)
{
    if (!value || !out) return false;
    if (!strcmp(value, "1") || !strcmp(value, "on") || !strcmp(value, "true")) {
        *out = 1; return true;
    }
    if (!strcmp(value, "0") || !strcmp(value, "off") || !strcmp(value, "false")) {
        *out = 0; return true;
    }
    return false;
}

static bool cviii_edition(CviiiModernHost *state, const char *name)
{
    if (!name) return false;
    int was_enhanced = state->enhanced;
    if (!strcmp(name, "Original") || !strcmp(name, "original")) {
        state->enhanced = 0;
        if (was_enhanced) cviii_layers_reset(&state->layers);
        return true;
    }
    if (!strcmp(name, "Enhanced") || !strcmp(name, "enhanced")) {
        state->enhanced = 1;
        if (!was_enhanced) cviii_layers_reset(&state->layers);
        return true;
    }
    return false;
}

static void cviii_load_setting(void *context, const char *key, const char *value)
{
    CviiiModernHost *state = (CviiiModernHost *)context;
    if (!key || !value) return;
    if (!strcmp(key, "Edition")) (void)cviii_edition(state, value);
    else if (!strcmp(key, "RoomView")) {
        if (!strcmp(value, "0")) (void)cviii_room_diag_mode(0);
        else if (!strcmp(value, "1")) (void)cviii_room_diag_mode(1);
        else if (!strcmp(value, "2")) (void)cviii_room_diag_mode(2);
    } else if (!strcmp(key, "UnlimitedSprites")) {
        int next;
        if (cviii_boolean(value, &next)) state->unlimited_sprites = next;
    } else if (!strcmp(key, "StableSprites")) {
        int next;
        if (cviii_boolean(value, &next)) {
            if (state->stable_sprites != next) cviii_layers_reset(&state->layers);
            state->stable_sprites = next;
        }
    } else (void)cviii_audio_levels_load(&state->volume, key, value);
}

static void cviii_save_settings(void *context, FILE *file)
{
    const CviiiModernHost *state = (const CviiiModernHost *)context;
    if (!file) return;
    fprintf(file, "Edition = %s\nUnlimitedSprites = %d\nStableSprites = %d\n",
            state->enhanced ? "Enhanced" : "Original",
            state->unlimited_sprites, state->stable_sprites);
    cviii_audio_levels_save(&state->volume, file);
    fprintf(file, "RoomView = %d\n", cviii_room_diag_get_mode());
}

static bool cviii_option(void *context, const char *name, const char *value)
{
    CviiiModernHost *state = (CviiiModernHost *)context;
    uint8_t percent;
    CviiiVolumeChannel channel;
    if (!name || !value) return false;
#ifdef CVIII_COLLISION_V11_SHADOW
    if (!strcmp(name, "--collision-v11-rom"))
        return CollisionDispatchV11_LoadRom(value) && CollisionDispatchV11_Enable();
    if (!strcmp(name, "--collision-v11-trace"))
        return CollisionDispatchV11_TraceOpen(value);
    if (!strcmp(name, "--collision-v11-dma-trace"))
        return CollisionDispatchV11_DmaTraceOpen(value);
#endif
    if (!strcmp(name, "--edition")) return cviii_edition(state, value);
    if (!strcmp(name, "--asset-index")) return cviii_room_diag_load_chr(value);
    if (!strcmp(name, "--room-resources")) return cviii_room_diag_load_rooms(value);
    if (!strcmp(name, "--room-graphics")) return cviii_room_diag_load_graphics(value);
    if (!strcmp(name, "--world-index")) return cviii_room_diag_index(value);
    if (!strcmp(name, "--camera-trace")) return cviii_room_diag_trace(value);
    if (!strcmp(name, "--cviii-diag-prefix")) return cviii_diag_runtime_open_5h8(value);
    if (!strcmp(name, "--cviii-diag-mode")) return cviii_diag_runtime_mode_5h8(value);
    if (!strcmp(name, "--room-source")) return cviii_room_diag_source(value);
    if (!strcmp(name, "--room-view")) {
        if (strlen(value)!=1 || value[0]<'0' || value[0]>'2') return false;
        return cviii_room_diag_mode(value[0]-'0');
    }
    if (!strcmp(name, "--room-preview")) {
        if (!strcmp(value,"1")) return cviii_room_diag_mode(1);
        if (!strcmp(value,"0")) return cviii_room_diag_mode(0);
        return false;
    }
    if (!strcmp(name, "--unlimited-sprites"))
        return cviii_boolean(value, &state->unlimited_sprites);
    if (!strcmp(name, "--stable-sprites")) {
        int next;
        if (!cviii_boolean(value, &next)) return false;
        if (next != state->stable_sprites) cviii_layers_reset(&state->layers);
        state->stable_sprites = next;
        return true;
    }
    if (!strcmp(name, "--master-volume")) channel = CVIII_VOLUME_MASTER;
    else if (!strcmp(name, "--music-volume")) channel = CVIII_VOLUME_MUSIC;
    else if (!strcmp(name, "--sfx-volume")) channel = CVIII_VOLUME_SFX;
    else return false;
    return cviii_audio_levels_parse(value, &percent) &&
           cviii_audio_levels_set(&state->volume, channel, (int)percent);
}

static void cviii_audio_mix(void *context, int16_t *samples, size_t count)
{
    CviiiModernHost *state = (CviiiModernHost *)context;
    if (state->enhanced)
        cviii_audio_levels_master_pcm(&state->volume, samples, count);
}

/* Use original machine-owned memory and PPU observations, presentation only.
 * Original and Enhanced capture the same optional background save-state
 * section, so switching editions cannot silently change save schemas. */
static void cviii_power_on(void *ctx)
{
    CviiiModernHost *s = (CviiiModernHost *)ctx;
    cviii_layers_reset(&s->layers);
    cviii_room_diag_boundary();
#ifdef CVIII_SELECTOR_TRACE_5H8S
    cviii_selector_trace_power_on_5h8s();
    cviii_selector_trace_power_on_5h8u(); /* CVIII-5H8U */
#endif /* CVIII-5H8S */
    cyc_render_capture_background(true);
}
static void cviii_state_loaded(void *ctx)
{
    cviii_layers_reset(&((CviiiModernHost *)ctx)->layers);
    cviii_room_diag_boundary();
}
static uint8_t cviii_chr(void *unused, uint16_t address)
{
    (void)unused;
    return cyc_render_chr(address);
}
static uint32_t cviii_color(void *unused, unsigned palette_index)
{
    (void)unused;
    return cyc_render_color((int)palette_index);
}
static const CviiiLayerFns cviii_layer_fns = {cviii_chr, cviii_color, NULL};
static void cviii_frame_end(void *ctx)
{
    CviiiModernHost *s = (CviiiModernHost *)ctx;
    cviii_room_diag_frame_end();
    cviii_diag_runtime_frame_5h8(cviii_room_diag_frame_id());
    if (!s->enhanced || !s->stable_sprites) return;
    const uint8_t *ram = cyc_cpu_ram();
    const uint8_t *oam = cyc_render_oam();
    if (!ram || !oam) return;
    uint8_t tall[CVIII_LAYER_H];
    uint16_t tables[CVIII_LAYER_H];
    for (int y = 0; y < CVIII_LAYER_H; ++y) {
        s->masks[y] = cyc_render_line_mask(y);
        tall[y] = (uint8_t)cyc_render_line_sprite16(y);
        tables[y] = cyc_render_line_sprite_table(y);
    }
    /* Authentic Japanese PRG26 OAM rotation phase is RAM $22. */
    cviii_layers_capture(&s->layers, oam, ram[0x22],
                         s->masks, tall, tables, &cviii_layer_fns);
}
static const uint32_t *cviii_sprite_present(void *ctx, int *width, int *height)
{
    CviiiModernHost *s = (CviiiModernHost *)ctx;
    if (!width || !height) return NULL;
    *width = CVIII_LAYER_W;
    *height = CVIII_LAYER_H;
    if (!s->enhanced || (!s->stable_sprites && !s->unlimited_sprites))
        return cyc_frame_argb();
    /* Incomplete background history must never produce a corrupt frame. */
    if (!cyc_render_background(s->presentation)) return cyc_frame_argb();
    if (s->stable_sprites && s->layers.primed) {
        cviii_layers_draw(&s->layers, s->presentation,
                          cyc_frame_bg_opaque(), s->masks, &cviii_layer_fns);
    } else {
        /* Existing OAM only. Cannot restore CPU-culled actors. */
        cyc_render_sprites(s->presentation, *width, *height, 0,
                           cyc_frame_bg_opaque(), NULL, NULL);
    }
    return s->presentation;
}

/* Room diagnostics are a final read-only presentation wrapper; Original
 * never changes size and the actual hardware picture is always authoritative. */
static const uint32_t *cviii_present(void *ctx, int *width, int *height)
{
    const CviiiModernHost *s=(const CviiiModernHost *)ctx;
    const uint32_t *native=cviii_sprite_present(ctx,width,height);
    if (!s->enhanced || !native || *width!=256 || *height!=240) return native;
    return cviii_room_diag_present(native,width,height);
}

/* The upstream menu owns pause, controls, save states, and rewind. Only
 * game-specific controls belong here. No SDL patches or save-file writes. */
#ifdef CYC_WITH_RECOMP_UI
static const RecompRuntimeUiItem cviii_menu_items[] = {
    {"cviii.edition", "CVIII", "Enhanced Edition",
     "Presentation and master gain only; original NES gameplay stays active.",
     RECOMP_RUNTIME_UI_BOOL, 0, 1, 1, NULL, 0, NULL},
    {"cviii.master", "CVIII", "Master volume",
     "Enhanced output gain (0-100); original VRC6 audio is unchanged.",
     RECOMP_RUNTIME_UI_INT, 0, 100, 1, NULL, 0, NULL},
    {"cviii.unlimited", "CVIII", "Unlimited OAM sprites (experimental)",
     "Display all available OAM pieces; does not simulate missing enemies.",
     RECOMP_RUNTIME_UI_BOOL, 0, 1, 1, NULL, 0, NULL},
    {"cviii.stable", "CVIII", "Stable sprites (experimental)",
     "JP0645 visual sprite continuity; not validated for every scene.",
     RECOMP_RUNTIME_UI_BOOL, 0, 1, 1, NULL, 0, NULL},
    {"cviii.room_view", "CVIII", "Room renderer diagnostics",
     "0=Off, 1=side-by-side, 2=426px inferred background. Not gameplay widescreen.",
     RECOMP_RUNTIME_UI_INT, 0, 2, 1, NULL, 0, NULL}
};

static int cviii_menu_get(void *context, const RecompRuntimeUiItem *item,
                          int *out)
{
    const CviiiModernHost *s = (const CviiiModernHost *)context;
    if (!s || !item || !item->key || !out) return 0;
    if (!strcmp(item->key, "cviii.edition")) *out = s->enhanced;
    else if (!strcmp(item->key, "cviii.master")) *out = s->volume.master;
    else if (!strcmp(item->key, "cviii.unlimited")) *out = s->unlimited_sprites;
    else if (!strcmp(item->key, "cviii.stable")) *out = s->stable_sprites;
    else if (!strcmp(item->key, "cviii.room_view")) *out = cviii_room_diag_get_mode();
    else return 0;
    return 1;
}

static int cviii_menu_set(void *context, const RecompRuntimeUiItem *item,
                          int value)
{
    CviiiModernHost *s = (CviiiModernHost *)context;
    if (!s || !item || !item->key) return 0;
    if (!strcmp(item->key, "cviii.edition")) {
        if (value != 0 && value != 1) return 0;
        if (s->enhanced != value) cviii_layers_reset(&s->layers);
        s->enhanced = value;
        return 1;
    }
    if (!strcmp(item->key, "cviii.room_view"))
        return cviii_room_diag_mode(value) ? 1 : 0;
    if (!strcmp(item->key, "cviii.master"))
        return cviii_audio_levels_set(&s->volume, CVIII_VOLUME_MASTER, value);
    if (value != 0 && value != 1) return 0;
    if (!strcmp(item->key, "cviii.unlimited")) {
        s->unlimited_sprites = value;
        return 1;
    }
    if (!strcmp(item->key, "cviii.stable")) {
        if (s->stable_sprites != value) cviii_layers_reset(&s->layers);
        s->stable_sprites = value;
        return 1;
    }
    return 0;
}

static const RecompRuntimeUiCallbacks cviii_menu_callbacks = {
    .context = &host,
    .get_value = cviii_menu_get,
    .set_value = cviii_menu_set
};
#endif

static const CycHostOption cviii_options[] = {
#ifdef CVIII_COLLISION_V11_SHADOW
    {"--collision-v11-rom", true, "Opt-in read-only FCDE dispatch shadow; authentic local ROM path"},
    {"--collision-v11-trace", true, "New private CSV of original NESRecomp FCDE entry/return states"},
    {"--collision-v11-dma-trace", true, "Additional original NESRecomp per-cycle DMA-count sidecar (requires --hash-out)"},
#endif
    {"--edition", true, "Original or Enhanced host mode (Original default)"},
    {"--unlimited-sprites", true, "EXPERIMENTAL: draw all OAM pieces (0/1)"},
    {"--stable-sprites", true, "EXPERIMENTAL: JP0645 OAM continuity (0/1)"},
    {"--master-volume", true, "Enhanced final PCM gain 0..100"},
    {"--music-volume", true, "Stored preference only (mixer not integrated)"},
    {"--sfx-volume", true, "Stored preference only (mixer not integrated)"},
    {"--asset-index", true, "Owner-local JP CHR cache"},
    {"--room-resources", true, "Owner-local JP room geometry cache"},
    {"--room-graphics", true, "Owner-local JP metatile cache"},
    {"--room-view", true, "0 off / 1 compare / 2 426px diagnostic"},
    {"--camera-trace", true, "Write one camera/room observation per presented NES frame (CSV)"},
    {"--cviii-diag-prefix", true, "Owner-local CPU/PPU/OAM evidence file prefix"},
    {"--cviii-diag-mode", true, "off / frames / full (RAM and CHR change snapshots)"},
    {"--world-index", true, "Owner-private Phase5H2 indexed level/world file"},
    {"--room-source", true, "atlas verified index / auto / room / ppu experimental"},
    {"--room-preview", true, "Historical alias: 1 compare or 0 off"}
};

static const CycHostExtras cviii_extras = {
    .ctx = &host,
    .present = cviii_present,
    .power_on = cviii_power_on,
    .state_loaded = cviii_state_loaded,
    .frame_end = cviii_frame_end,
    .load_setting = cviii_load_setting,
    .save_settings = cviii_save_settings,
    .options = cviii_options,
    .option_count = sizeof(cviii_options) / sizeof(cviii_options[0]),
    .option = cviii_option,
    .audio_mix = cviii_audio_mix
#ifdef CYC_WITH_RECOMP_UI
    , .menu_items = cviii_menu_items
    , .menu_item_count = sizeof(cviii_menu_items) / sizeof(cviii_menu_items[0])
    , .menu_callbacks = &cviii_menu_callbacks
#endif
};

const CycHostExtras *cyc_host_extras(void)
{
#ifdef CVIII_COLLISION_V11_SHADOW
    /* This is called by option discovery before cyc_hooks_validate() when
     * --collision-v11-rom is supplied. No flag means no runtime hook use. */
    static int registered;
    if (!registered) {
        if (!CollisionDispatchV11_Register()) {
            fprintf(stderr,"Collision v11: mod hook registration rejected\n");
            return NULL;
        }
        registered=1;
    }
#endif
    return &cviii_extras;
}
