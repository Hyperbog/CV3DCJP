#!/usr/bin/env python3
"""JP0620 owner-local SDL overlay: F6 original-CHR audio menu, safely paused.

Layer over JP0591 -> JP0594 -> JP0596; never edit pinned external NESRecomp.
Audio slider state/drawing lives in game-owned native C11. The SDL overlay
only routes keyboard/controller events and respects the host pause barrier.
"""
from __future__ import annotations
import argparse
from pathlib import Path

MARKER="CVIII_AUDIO_MENU_0620"
SOURCE_NAME="cviii_cyc_sdl_stage_bootstrap.c"
OUTPUT_NAME="cviii_cyc_sdl_audio_menu.c"
OPEN_START="        bool open = menu_open() || cviii_stage_open;"
OPEN_FINAL="\n        open = menu_open() || cviii_stage_open;"
KEY="                if ((!open || cviii_stage_open) && cviii_stage_key(&ev.key)) {"
CONTROLLER="                menu_nav_button(ev.cbutton.button);"
SHELL=r'''
/* CVIII_AUDIO_MENU_0620: host-only modal gate and input routing.
 * Game-owned C functions never receive SDL pointers or modify CPU input. */
#include "cviii_audio_menu_0620.h"
static bool cviii_audio_menu_toggled;
static bool cviii_audio_menu_key(const SDL_KeyboardEvent *key)
{
    SDL_Scancode sc=key->keysym.scancode;
    if (sc==SDL_SCANCODE_F6 && !key->repeat) {
#ifdef NESRECOMP_NET
        if (nes_netplay_active()) {
            note("AUDIO OPTIONS", "Unavailable during netplay");
            return true;
        }
#endif
        if (!cviii_audio_menu_toggle()) {
            note("AUDIO OPTIONS", "Enhanced and authenticated CHR assets required");
        }
        cviii_audio_menu_toggled=true;
        return true;
    }
    if (!cviii_audio_menu_active()) return false;
    if (sc==SDL_SCANCODE_ESCAPE || sc==SDL_SCANCODE_BACKSPACE) {
        if (!key->repeat) {
            cviii_audio_menu_close();
            cviii_audio_menu_toggled=true;
        }
        return true;
    }
    if (sc==SDL_SCANCODE_UP || sc==SDL_SCANCODE_DOWN) {
        if (!key->repeat)
            (void)cviii_audio_menu_move(sc==SDL_SCANCODE_UP?-1:1);
        return true;
    }
    if (sc==SDL_SCANCODE_LEFT || sc==SDL_SCANCODE_RIGHT) {
        if (cviii_audio_menu_move(sc==SDL_SCANCODE_LEFT?-2:2))
            save_settings();
        return true;
    }
    return true; /* swallow other keys while the modal owns input */
}
static void cviii_audio_menu_button(int button)
{
    if (!cviii_audio_menu_active()) return;
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        (void)cviii_audio_menu_move(-1);
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        (void)cviii_audio_menu_move(1);
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        if (cviii_audio_menu_move(-2)) save_settings();
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        if (cviii_audio_menu_move(2)) save_settings();
        break;
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_START:
        cviii_audio_menu_close();
        cviii_audio_menu_toggled=true;
        break;
    default: break;
    }
}
'''

def once(src:str,old:str,new:str)->str:
    if src.count(old)!=1:
        raise ValueError("Pinned JP0596 SDL overlay anchor changed: "+repr(old[:85]))
    return src.replace(old,new,1)

def generate(src:str)->str:
    if MARKER in src or "CVIII_STAGE_WARP_0594" not in src:
        raise ValueError("JP0620 requires one unmodified JP0596 SDL overlay")
    src=once(src,"static bool cviii_stage_open;",
             SHELL+"\nstatic bool cviii_stage_open;")
    src=once(src,KEY,
'''                /* F6 modal audio consumes events before the F10 debug menu.
                 * The usual native host Menu remains independent. */
                if ((cviii_audio_menu_active() || (!open && !cviii_stage_open)) &&
                    cviii_audio_menu_key(&ev.key)) {
                    open=menu_open() || cviii_stage_open || cviii_audio_menu_active();
                    continue;
                }
'''+KEY)
    src=once(src,CONTROLLER,
'''                if (cviii_audio_menu_active()) {
                    cviii_audio_menu_button(ev.cbutton.button);
                    continue;
                }
'''+CONTROLLER)
    src=once(src,OPEN_START,
'''        cviii_audio_menu_toggled=false;
        bool open = menu_open() || cviii_stage_open || cviii_audio_menu_active();''')
    src=once(src,OPEN_FINAL,
'''\n        open = menu_open() || cviii_stage_open || cviii_audio_menu_active();
        if (cviii_audio_menu_toggled && open && dev)
            SDL_ClearQueuedAudio(dev); /* no queued SFX bleeding into pause */''')
    src=once(src,"if (edge[CYC_SC_MENU] && s_have_menu)",
             "if (edge[CYC_SC_MENU] && s_have_menu && !cviii_audio_menu_active() && !cviii_audio_menu_toggled)")
    src=once(src,"if (edge[CYC_SC_SAVE_STATE] && !menu_open() && !cviii_stage_open)",
             "if (edge[CYC_SC_SAVE_STATE] && !menu_open() && !cviii_stage_open && !cviii_audio_menu_active())")
    src=once(src,"if (edge[CYC_SC_LOAD_STATE] && !menu_open() && !cviii_stage_open)",
             "if (edge[CYC_SC_LOAD_STATE] && !menu_open() && !cviii_stage_open && !cviii_audio_menu_active())")
    return src

def smoke()->None:
    stub="/* CVIII_STAGE_WARP_0594 */\n"+"\n".join((
        "static bool cviii_stage_open;",KEY,CONTROLLER,OPEN_START,OPEN_FINAL,
        "if (edge[CYC_SC_MENU] && s_have_menu)",
        "if (edge[CYC_SC_SAVE_STATE] && !menu_open() && !cviii_stage_open)",
        "if (edge[CYC_SC_LOAD_STATE] && !menu_open() && !cviii_stage_open)"
    ))
    patched=generate(stub)
    assert patched.count(MARKER)==1
    assert "SDL_SCANCODE_F6" in patched
    assert "SDL_CONTROLLER_BUTTON_DPAD_RIGHT" in patched
    assert "SDL_ClearQueuedAudio(dev)" in patched
    assert "open = menu_open() || cviii_stage_open || cviii_audio_menu_active()" in patched
    assert "cviii_audio_menu_toggled=false" in patched
    assert "if (cviii_audio_menu_move(2)) save_settings();" in patched
    assert "cviii_audio_menu_key(&ev.key)" in patched
    assert "cviii_audio_menu_active())" in patched
    for bad in (patched,stub.replace(KEY,"/* changed SDL event */"),
                stub.replace(OPEN_FINAL,"/* changed pause semantics */")):
        try:generate(bad)
        except ValueError:continue
        raise AssertionError("Unpinned SDL overlay accepted")
    print("PASS JP0620 modal original-CHR F6 keyboard/controller and pause anchors")

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source",type=Path)
    p.add_argument("--out",type=Path)
    p.add_argument("--smoke",action="store_true")
    a=p.parse_args()
    if a.smoke:smoke();return
    if not a.source or not a.out:p.error("--source and --out required")
    source=a.source.resolve(strict=True);out=a.out.resolve()
    if source.name!=SOURCE_NAME or out.name!=OUTPUT_NAME or (
            source.parent!=out.parent or source==out):
        raise ValueError("JP0620 expects adjacent final overlays in external SDL checkout")
    out.write_text(generate(source.read_text(encoding="utf-8")),encoding="utf-8")
    print("PASS JP0620 private SDL modal audio menu patch: "+str(out))
if __name__=="__main__":main()
