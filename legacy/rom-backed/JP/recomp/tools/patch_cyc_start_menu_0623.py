#!/usr/bin/env python3
"""JP0623: add original-CHR F4 Start/Pause page to the pinned private SDL chain.

Input: JP0620 generated cviii_cyc_sdl_audio_menu.c.
Output: private cviii_cyc_sdl_start_menu.c alongside external NESRecomp.
Existing save-state slots and audio modal remain the engine of each action.
"""
from __future__ import annotations
import argparse
from pathlib import Path

MARKER="CVIII_START_PAUSE_0623"
KEY='                /* F6 modal audio consumes events before the F10 debug menu.'
PAD='                if (cviii_audio_menu_active()) {\n                    cviii_audio_menu_button(ev.cbutton.button);'
DEV='''#ifdef CYC_DEV_UI
                if (!ev.key.repeat
#ifdef NESRECOMP_NET
                    && !nes_netplay_active()
#endif
                    && dev_key(ev.key.keysym.scancode)) continue;
#endif
'''
FRAME_START='        bool open = menu_open() || cviii_stage_open || cviii_audio_menu_active();'
FRAME_FINAL='        open = menu_open() || cviii_stage_open || cviii_audio_menu_active();'
SHELL=r'''
/* CVIII_START_PAUSE_0623: engine-owned actions, game-owned stable menu IDs.
 * No cartridge writes, no alternative save-state serializer. */
#include "cviii_start_host_0623.h"
#include "cviii_start_menu_0623.h"
static bool save_state_slot(void);
static bool load_state_slot(void);
static void cviii_pause_action(int action)
{
    switch (action) {
    case CVIII_START_SAVE:
        (void)save_state_slot();
        break;
    case CVIII_START_LOAD:
        (void)load_state_slot();
        break;
    case CVIII_START_AUDIO:
        cviii_pause_close();
        if (!cviii_audio_menu_toggle())
            note("AUDIO", "Original CHR assets unavailable");
        cviii_audio_menu_toggled=true;
        break;
    case CVIII_START_QUIT:
        /* Two successive Accept actions on Quit, enforced by registry. */
        s_running=false;
        break;
    default: break; /* no executable external mod callback */
    }
}
static bool cviii_pause_key(const SDL_KeyboardEvent *key)
{
    const SDL_Scancode sc=key->keysym.scancode;
    if (sc==SDL_SCANCODE_F4) {
        if(!key->repeat) {
#ifdef NESRECOMP_NET
            if(nes_netplay_active()) {
                note("PAUSE", "Unavailable during netplay");
                return true;
            }
#endif
            if(!cviii_pause_toggle())
                note("PAUSE", "Enhanced and original CHR assets required");
            cviii_audio_menu_toggled=true; /* clear pending audio on open */
        }
        return true;
    }
    if(!cviii_pause_active()) return false;
    if(sc==SDL_SCANCODE_ESCAPE || sc==SDL_SCANCODE_BACKSPACE) {
        if(!key->repeat) {
            cviii_pause_close();
            cviii_audio_menu_toggled=true;
        }
        return true;
    }
    if(sc==SDL_SCANCODE_UP || sc==SDL_SCANCODE_DOWN) {
        if(!key->repeat)
            cviii_pause_move(sc==SDL_SCANCODE_UP?-1:1);
        return true;
    }
    if(sc==SDL_SCANCODE_RETURN || sc==SDL_SCANCODE_KP_ENTER ||
       sc==SDL_SCANCODE_SPACE) {
        if(!key->repeat)cviii_pause_action(cviii_pause_accept());
        return true;
    }
    return true; /* modal consumes other keys, never forwards to game */
}
static void cviii_pause_button(int button)
{
    if(!cviii_pause_active())return;
    switch(button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: cviii_pause_move(-1); break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: cviii_pause_move(1); break;
    case SDL_CONTROLLER_BUTTON_A: cviii_pause_action(cviii_pause_accept()); break;
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_START:
        cviii_pause_close();
        cviii_audio_menu_toggled=true;
        break;
    default: break;
    }
}
'''
def once(src:str,old:str,new:str)->str:
    if src.count(old)!=1:
        raise ValueError("JP0623 pinned private SDL overlay anchor: "+repr(old[:100]))
    return src.replace(old,new,1)
def generate(src:str)->str:
    if MARKER in src or src.count("CVIII_AUDIO_MENU_0620")!=1 or \
       "CVIII_PASSWORD_BOOTSTRAP_0596" not in src:
        raise ValueError("JP0623 requires unmodified JP0620 SDL audio overlay")
    src=once(src,"static bool cviii_stage_open;",SHELL+"\nstatic bool cviii_stage_open;")
    # Developer F6-Fn toggle must not swallow F6 Audio, F4 Pause or F10
    # password bootstrap. Keep the old handler, after modal dispatch.
    src=once(src,DEV,"")
    src=once(src,KEY,
'''                if ((cviii_pause_active() ||
                    (!open && !cviii_stage_open && !cviii_audio_menu_active())) &&
                    cviii_pause_key(&ev.key)) {
                    open=menu_open() || cviii_stage_open ||
                         cviii_audio_menu_active() || cviii_pause_active();
                    continue;
                }
'''+KEY)
    src=once(src,"                if (open) menu_nav_key(&ev.key);",
             DEV+"                if (open) menu_nav_key(&ev.key);")
    src=once(src,PAD,
'''                if (cviii_pause_active()) {
                    cviii_pause_button(ev.cbutton.button);
                    continue;
                }
'''+PAD)
    src=once(src,FRAME_START,
             FRAME_START.replace(";"," || cviii_pause_active();"))
    src=once(src,FRAME_FINAL,
             FRAME_FINAL.replace(";"," || cviii_pause_active();"))
    src=once(src,
        'if (edge[CYC_SC_MENU] && s_have_menu && !cviii_audio_menu_active() && !cviii_audio_menu_toggled)',
        'if (edge[CYC_SC_MENU] && s_have_menu && !cviii_audio_menu_active() && !cviii_pause_active() && !cviii_audio_menu_toggled)')
    for action in ("SAVE","LOAD"):
        old=f'if (edge[CYC_SC_{action}_STATE] && !menu_open() && !cviii_stage_open && !cviii_audio_menu_active())'
        src=once(src,old,old[:-1]+' && !cviii_pause_active())')
    return src
def smoke()->None:
    stub="\n".join((
        "/* CVIII_AUDIO_MENU_0620 */","/* CVIII_PASSWORD_BOOTSTRAP_0596 */",
        "static bool cviii_stage_open;",DEV,KEY,
        "                if (open) menu_nav_key(&ev.key);",
        PAD,FRAME_START,FRAME_FINAL,
        "if (edge[CYC_SC_MENU] && s_have_menu && !cviii_audio_menu_active() && !cviii_audio_menu_toggled)",
        "if (edge[CYC_SC_SAVE_STATE] && !menu_open() && !cviii_stage_open && !cviii_audio_menu_active())",
        "if (edge[CYC_SC_LOAD_STATE] && !menu_open() && !cviii_stage_open && !cviii_audio_menu_active())",
    ))
    result=generate(stub)
    assert result.count(MARKER)==1
    assert result.count("dev_key(ev.key.keysym.scancode)") == 1
    assert result.index("cviii_pause_key(&ev.key)") < result.index("dev_key(ev.key.keysym.scancode)")
    assert "cviii_pause_action(cviii_pause_accept())" in result
    assert "cviii_start_host_0623.h" in result
    assert "cviii_pause_active() &&" in result
    assert "|| cviii_pause_active();" in result
    assert "case CVIII_START_QUIT:" in result
    for bad in (result,stub.replace(PAD,"/* changed controller */")):
        try:generate(bad)
        except ValueError:continue
        raise AssertionError("Already-patched/drifted host source accepted")
    print("PASS JP0623 five-layer Pause/Audio native SDL integration anchors")
def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source",type=Path)
    p.add_argument("--out",type=Path)
    p.add_argument("--smoke",action="store_true")
    a=p.parse_args()
    if a.smoke:smoke();return
    if not a.source or not a.out:p.error("Use --source and --out")
    source=a.source.resolve(strict=True)
    out=a.out.resolve()
    if source.name!="cviii_cyc_sdl_audio_menu.c" or \
       out.name!="cviii_cyc_sdl_start_menu.c" or \
       source.parent!=out.parent or source==out:
        raise ValueError("JP0623 requires adjacent owner-private final SDL overlays")
    out.write_text(generate(source.read_text(encoding="utf-8")),encoding="utf-8")
    print("PASS JP0623 Start/Pause overlay: "+str(out))
if __name__=="__main__":main()
