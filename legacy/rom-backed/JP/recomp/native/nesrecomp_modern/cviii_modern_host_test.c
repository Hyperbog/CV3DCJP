/* Executable host API contract test. No ROM contents or SDL2 required. */
#include "cyc_host_extras.h"
#include "cyc_core.h"
#ifdef CYC_WITH_RECOMP_UI
#include "recomp_runtime_ui.h"
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const CycHostExtras *h = cyc_host_extras();
    assert(h && h->ctx && h->load_setting && h->save_settings);
    /* Verify required option identities, not a brittle fixed array length.
     * Future optional diagnostics may extend the host option table. */
    assert(h->option && h->audio_mix && h->options);
    {
        static const char * const required[] = {
            "--edition", "--unlimited-sprites", "--stable-sprites",
            "--master-volume", "--music-volume", "--sfx-volume",
            "--asset-index", "--room-resources", "--room-graphics",
            "--room-view", "--camera-trace", "--cviii-diag-prefix",
            "--cviii-diag-mode", "--world-index", "--room-source",
            "--room-preview"
        };
        const size_t required_count = sizeof(required) / sizeof(required[0]);
        assert(h->option_count >= required_count);
        for (size_t i = 0; i < required_count; ++i) {
            size_t found = 0;
            for (size_t j = 0; j < h->option_count; ++j) {
                assert(h->options[j].name && h->options[j].name[0]);
                if (!strcmp(h->options[j].name, required[i])) {
                    assert(h->options[j].takes_value);
                    ++found;
                }
            }
            assert(found == 1); /* Missing or duplicate option is a regression. */
        }
    }
    assert(h->present && h->power_on && h->state_loaded && h->frame_end);
    h->power_on(h->ctx);
    assert(!h->option(h->ctx,"--room-view","3"));
    assert(!h->option(h->ctx,"--room-view","10"));
    assert(!h->option(h->ctx,"--room-preview","2"));
    assert(!h->option(h->ctx,"--camera-trace",""));
    assert(h->option(h->ctx,"--room-view","1"));
    /* With no authenticated resource packs the visual is unchanged. */
    { int w=0,hh=0; const uint32_t *image=h->present(h->ctx,&w,&hh);
      assert(image==cyc_frame_argb() && w==256 && hh==240); }
    assert(h->option(h->ctx,"--room-view","0"));
    assert(!h->option(h->ctx,"--room-source","bogus"));
    assert(h->option(h->ctx,"--room-source","auto"));
    assert(h->option(h->ctx,"--room-source","atlas"));
    assert(!h->option(h->ctx,"--world-index","__missing_world.idx"));
    assert(h->option(h->ctx,"--room-source","auto"));
    assert(h->option(h->ctx,"--edition","enhanced"));
    assert(h->option(h->ctx,"--room-view","2"));
    { int w=0,hh=0; const uint32_t *img=h->present(h->ctx,&w,&hh);
      assert(img!=cyc_frame_argb() && w==426 && hh==240);
      /* The native frame is at pixel 85, never modified by the side renderer. */
      assert(img[85]==cyc_frame_argb()[0]);
    }
    assert(h->option(h->ctx,"--room-view","0"));
    assert(h->option(h->ctx,"--edition","original"));
    assert(!h->option(h->ctx, "--stable-sprites", "garbage"));
    assert(!h->option(h->ctx, "--unlimited-sprites", "2"));
    assert(h->option(h->ctx, "--stable-sprites", "0"));
    assert(h->option(h->ctx, "--unlimited-sprites", "0"));
    { int width = 0, height = 0;
      const uint32_t *orig = h->present(h->ctx, &width, &height);
      assert(orig == cyc_frame_argb() && width == 256 && height == 240);
    }
    assert(!h->option(h->ctx, "--edition", "invalid"));
    assert(!h->option(h->ctx, "--master-volume", "101"));
    assert(!h->option(h->ctx, "--master-volume", "-5"));
    assert(h->option(h->ctx, "--master-volume", "50"));
    int16_t original[] = {-101, 0, 101};
    assert(h->option(h->ctx, "--edition", "Original"));
    h->audio_mix(h->ctx, original, 3);
    assert(original[0] == -101 && original[1] == 0 && original[2] == 101);
    int16_t enhanced[] = {-101, 0, 101};
    assert(h->option(h->ctx, "--edition", "Enhanced"));
    h->audio_mix(h->ctx, enhanced, 3);
    assert(enhanced[0] == -50 && enhanced[1] == 0 && enhanced[2] == 50);
    assert(h->option(h->ctx, "--music-volume", "35"));
    assert(h->option(h->ctx, "--sfx-volume", "87"));

    FILE *f = tmpfile();
    assert(f);
    h->save_settings(h->ctx, f);
    rewind(f);
    char text[256] = {0};
    size_t n = fread(text, 1, sizeof(text)-1, f);
    fclose(f);
    assert(n > 0);
    assert(strstr(text, "Edition = Enhanced\n"));
    assert(strstr(text, "UnlimitedSprites = 0\n"));
    assert(strstr(text, "StableSprites = 0\n"));
    assert(strstr(text, "MasterVolume = 50\n"));
    assert(strstr(text, "MusicVolume = 35\n"));
    assert(strstr(text, "SoundEffectsVolume = 87\n"));
    assert(strstr(text, "RoomView = 0\n"));

    /* New presentation choices must not alter the Original hardware frame. */
    assert(h->option(h->ctx, "--unlimited-sprites", "1"));
    assert(h->option(h->ctx, "--stable-sprites", "1"));
    h->load_setting(h->ctx, "Edition", "Original");
    { int w=0,hh=0;
      const uint32_t *original = h->present(h->ctx, &w, &hh);
      assert(original == cyc_frame_argb());
    }
    /* Enhanced uses the game's own presentation buffer; save-load resets it. */
    assert(h->option(h->ctx, "--edition", "Enhanced"));
    { int w=0,hh=0;
      const uint32_t *present = h->present(h->ctx, &w, &hh);
      assert(present != cyc_frame_argb());
      assert(w == 256 && hh == 240);
    }
    h->state_loaded(h->ctx);
    h->load_setting(h->ctx, "Edition", "Original");
    h->load_setting(h->ctx, "MasterVolume", "20");
    int16_t unchanged[] = {100};
    h->audio_mix(h->ctx, unchanged, 1);
    assert(unchanged[0] == 100);
    assert(h->option(h->ctx, "--edition", "enhanced"));
    h->audio_mix(h->ctx, unchanged, 1);
    assert(unchanged[0] == 20);
#ifdef CYC_WITH_RECOMP_UI
    assert(h->menu_items && h->menu_callbacks && h->menu_item_count == 5);
    const RecompRuntimeUiCallbacks *cb = h->menu_callbacks;
    const RecompRuntimeUiItem *mode = &h->menu_items[0];
    const RecompRuntimeUiItem *master = &h->menu_items[1];
    int value = -1;
    assert(!strcmp(mode->key, "cviii.edition"));
    assert(!strcmp(master->key, "cviii.master"));
    const RecompRuntimeUiItem *unlimited = &h->menu_items[2];
    const RecompRuntimeUiItem *stable = &h->menu_items[3];
    assert(!strcmp(unlimited->key, "cviii.unlimited"));
    assert(!strcmp(stable->key, "cviii.stable"));
    const RecompRuntimeUiItem *room = &h->menu_items[4];
    assert(!strcmp(room->key,"cviii.room_view"));
    assert(!cb->set_value(cb->context, room, 3));
    assert(cb->set_value(cb->context, room, 2));
    assert(cb->get_value(cb->context, room, &value) && value == 2);
    assert(cb->set_value(cb->context, room, 0));
    assert(!cb->set_value(cb->context, stable, 2));
    assert(cb->set_value(cb->context, stable, 0));
    assert(cb->set_value(cb->context, unlimited, 0));
    assert(cb->get_value(cb->context, stable, &value) && value == 0);
    assert(cb->get_value(cb->context, mode, &value) && value == 1);
    assert(cb->get_value(cb->context, master, &value) && value == 20);
    assert(!cb->set_value(cb->context, mode, 3));
    assert(!cb->set_value(cb->context, master, 101));
    assert(cb->set_value(cb->context, mode, 0));
    assert(cb->set_value(cb->context, master, 65));
    assert(cb->get_value(cb->context, mode, &value) && value == 0);
    assert(cb->get_value(cb->context, master, &value) && value == 65);
    int16_t raw[] = {100};
    h->audio_mix(h->ctx, raw, 1);
    assert(raw[0] == 100); /* Original mode must not alter sound */
    assert(cb->set_value(cb->context, mode, 1));
    h->audio_mix(h->ctx, raw, 1);
    assert(raw[0] == 65);
#endif
    puts("PASS CVIII modern host extras: original/Enhanced isolation, PCM, options, persistence");
    return 0;
}
