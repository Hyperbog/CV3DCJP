#!/usr/bin/env python3
"""JP0644 executable seventh-layer Pause/Save/Load/Audio shell, ROM-free.

Assemble the actual JP0623→JP0625→JP0626 SDL C fragments with the real
game-owned action/slot/audio/menu-stack C modules. Platform callbacks and
host save file I/O are simulated; no commercial ROM or gameplay is run.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'decomp'/'tools'))
import jp_slot_shell_test_0625 as slot

TOOLS = Path(__file__).resolve().parent
ENHANCED = TOOLS.parent / "native" / "enhanced"

FAKE_FRAME_RECOMPUTE = r'''
/* The seventh source patch also affects the SDL F6 return event batch. */
static bool open;
static bool cviii_stage_open;
static bool menu_open(void) { return false; }
static int cviii_audio_menu_active(void) { return audio_state.open; }
static void cviii_audio_event_recompute(void)
{
    for (int iteration=0;iteration<1;iteration++) {
                    open=menu_open() || cviii_stage_open || cviii_audio_menu_active();
                    continue;
    }
}
'''

MAIN = r'''
static bool save_ok=true,load_ok=true;
static int saved_slot=-1,loaded_slot=-1;
static bool save_state_slot(void)
{
    save_calls++;
    saved_slot=(int)cviii_save_slot;
    return save_ok;
}
static bool load_state_slot(void)
{
    load_calls++;
    loaded_slot=(int)cviii_save_slot;
    return load_ok;
}
static SDL_KeyboardEvent key(SDL_Scancode sc,int repeat)
{
    SDL_KeyboardEvent e={{sc},repeat};
    return e;
}
int main(void)
{
    assert(cviii_start_defaults(&game_menu));
    cviii_start_seal(&game_menu);
    SDL_KeyboardEvent e=key(SDL_SCANCODE_F4,0);
#ifdef NESRECOMP_NET
    netplay_on=1;
    assert(cviii_pause_key(&e) && !cviii_pause_active());
    netplay_on=0;
#endif
    enhanced=0;
    assert(cviii_pause_key(&e) && !cviii_pause_active());
    enhanced=1;
    assets_ok=0;
    assert(cviii_pause_key(&e) && !cviii_pause_active());
    assets_ok=1;

    /* Failed SAVE: preserve quick-slot 0 and reopen selected child at 1. */
    assert(cviii_pause_key(&e) && cviii_pause_active());
    assert(cviii_pause_active() && !audio_state.open);
    e=key(SDL_SCANCODE_RETURN,0);
    assert(cviii_pause_key(&e) && cviii_pause_slot_active());
    e=key(SDL_SCANCODE_RIGHT,0);
    assert(cviii_pause_key(&e) && slot_picker.slot==1u);
    save_ok=false;
    e=key(SDL_SCANCODE_RETURN,0);
    assert(cviii_pause_key(&e) && save_calls==1 && saved_slot==1);
    assert(cviii_save_slot==0u && cviii_pause_active());
    assert(cviii_pause_slot_active() && slot_picker.slot==1u);
    assert(!audio_state.open && s_running);
    save_ok=true;
    assert(cviii_pause_key(&e) && save_calls==2 && saved_slot==1);
    assert(cviii_save_slot==1u && !cviii_pause_active());
    assert(!cviii_pause_slot_active() && !game_menu.open);
    assert(!cviii_pause_key(&e)); /* game input returns only after dismissal */

    /* Failed LOAD: original quick slot unchanged, B cancels child. */
    e=key(SDL_SCANCODE_F4,0);
    assert(cviii_pause_key(&e) && cviii_pause_active());
    cviii_pause_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_A);
    assert(cviii_pause_slot_active() && slot_picker.slot==1u);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    assert(slot_picker.slot==2u);
    load_ok=false;
    cviii_pause_button(SDL_CONTROLLER_BUTTON_A);
    assert(load_calls==1 && loaded_slot==2);
    assert(cviii_save_slot==1u && cviii_pause_active());
    assert(cviii_pause_slot_active() && slot_picker.slot==2u);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_B);
    assert(cviii_pause_active() && !cviii_pause_slot_active());
    assert(cviii_save_slot==1u);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_A);
    assert(cviii_pause_slot_active() && slot_picker.slot==1u);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    load_ok=true;
    cviii_pause_button(SDL_CONTROLLER_BUTTON_A);
    assert(load_calls==2 && loaded_slot==2 && cviii_save_slot==2u);
    assert(!cviii_pause_active());

    /* Nested audio: parent is resident, but child owns all modal input. */
    e=key(SDL_SCANCODE_F4,0);
    assert(cviii_pause_key(&e) && cviii_pause_active());
    cviii_pause_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    assert(game_menu.selected==2u);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_A);
    assert(audio_calls==1 && audio_state.open && game_menu.open);
    assert(!cviii_pause_active() && cviii_audio_menu_active());
    assert(cviii_audio_menu_toggled && s_running);
    /* F4 must not discard a resident parent under the Audio child. */
    assert(cviii_pause_key(&e) && game_menu.open && audio_state.open);
    assert(cviii_audio_menu_state_move(&audio_state,&audio_levels,1)==0);
    assert(audio_state.selected==1u);
    assert(cviii_audio_menu_state_move(&audio_state,&audio_levels,-2)==1);
    assert(audio_levels.music==99u && audio_levels.master==100u);
    cviii_audio_menu_state_close(&audio_state); /* F6/Escape/B/Start */
    cviii_audio_event_recompute();
    assert(open && game_menu.open && cviii_pause_active());
    assert(game_menu.selected==2u && !audio_state.open);
    e=key(SDL_SCANCODE_A,0);
    assert(cviii_pause_key(&e)); /* restored parent still owns game input */
    e=key(SDL_SCANCODE_F4,0);
    assert(cviii_pause_key(&e) && !cviii_pause_active());
    assert(!game_menu.open && !open==false); /* last F6 recompute was paused */
    e=key(SDL_SCANCODE_A,0);
    assert(!cviii_pause_key(&e)); /* gameplay input restored on parent close */

    /* Standalone F6 opens without a parent and exits to gameplay. */
    assert(cviii_audio_menu_toggle() && audio_state.open && !game_menu.open);
    cviii_audio_menu_state_close(&audio_state);
    assert(!audio_state.open && !cviii_pause_active());

    /* Double-confirm Quit remains protected and distinct from Audio. */
    e=key(SDL_SCANCODE_F4,0);
    assert(cviii_pause_key(&e) && cviii_pause_active());
    cviii_pause_button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    assert(game_menu.selected==3u);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_A);
    assert(s_running && game_menu.quit_armed);
    cviii_pause_button(SDL_CONTROLLER_BUTTON_A);
    assert(!s_running);
    e=key(SDL_SCANCODE_ESCAPE,0);
    assert(cviii_pause_key(&e) && !cviii_pause_active());
    assert(save_calls==2 && load_calls==2 && audio_calls==1);
    puts("PASS JP0644 7-layer F4/Save/Load/Audio parent return and failure rollback");
    return 0;
}
'''

def fixture() -> str:
    parent = slot.imp(TOOLS / "patch_cyc_start_menu_0623.py")
    picker = slot.imp(TOOLS / "patch_cyc_slot_picker_0625.py")
    audio = slot.imp(TOOLS / "patch_cyc_audio_return_0626.py")
    sixth = picker.generate(parent.SHELL)
    seventh = audio.generate(sixth + "\n" + FAKE_FRAME_RECOMPUTE)
    if seventh.count(audio.MARKER) != 1:
        raise ValueError("expected exact seventh SDL source layer")
    full = slot.fixture()
    if full.count(sixth) != 1 or full.count(slot.END) != 1:
        raise ValueError("failed to find actual generated sixth shell fixture")
    full = full.replace(sixth, seventh, 1).replace(slot.END, MAIN, 1)
    # Use the real game-owned audio state and nested menu-stack implementation
    # instead of the JP0624 demo stub.
    stub = """static int audio_open=0;"""
    full = full.replace(stub, """#include "cviii_menu_stack_0626.h"
static CviiiAudioMenuState audio_state;
static CviiiAudioLevels audio_levels=CVIII_AUDIO_LEVELS_DEFAULT;""", 1)
    old_toggle = """static int cviii_audio_menu_toggle(void)
{
 if(!enhanced || !assets_ok)return 0;
 audio_open=!audio_open;
 return 1;
}"""
    new_toggle = """static int cviii_audio_menu_toggle(void)
{
 return cviii_audio_menu_state_toggle(&audio_state,enhanced,assets_ok);
}"""
    if full.count(old_toggle) != 1:
        raise ValueError("original F6 demo toggle changed")
    full = full.replace(old_toggle,new_toggle,1)
    full = full.replace("int cviii_pause_active(void){return game_menu.open;}",
       "int cviii_pause_active(void){return cviii_menu_stack_parent_active(&game_menu,&audio_state);}",1)
    full = full.replace(
       " return cviii_start_toggle(&game_menu,enhanced,assets_ok);",
       " if(audio_state.open)return 0;\n return cviii_start_toggle(&game_menu,enhanced,assets_ok);",1)
    helper = """int cviii_pause_audio_enter(void)
{
 audio_calls++;
 return cviii_menu_stack_enter_audio(&game_menu,&audio_state,enhanced,assets_ok);
}
"""
    if full.count("static CviiiSlotPicker slot_picker;") != 1:
        raise ValueError("missing game-owned slot picker shim")
    full = full.replace("static CviiiSlotPicker slot_picker;",
                        helper + "static CviiiSlotPicker slot_picker;",1)
    if "audio_open" in full:
        raise ValueError("left old audio boolean instead of nested model")
    return full


def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--smoke",action="store_true")
    p.add_argument("--cc",default=os.getenv("CC","cc"))
    p.add_argument("--emit-c",type=Path)
    a=p.parse_args()
    code=fixture()
    required=("CVIII_START_PAUSE_0623","CVIII_SLOT_SELECTOR_0625",
              "CVIII_AUDIO_RETURN_0626","cviii_pause_audio_enter()",
              "cviii_save_slot=previous;","cviii_pause_slot_begin(action,selected)",
              "cviii_menu_stack_parent_active","cviii_audio_event_recompute")
    for marker in required:
        if marker not in code:
            raise ValueError("Missing true generated action marker: "+marker)
    if a.smoke:
        print("PASS JP0644 seventh generated SDL shell extraction, modal/slot contracts")
        return
    if a.emit_c:
        a.emit_c.write_text(code,encoding="utf-8")
        return
    with tempfile.TemporaryDirectory(prefix="jp0644_menu_") as d:
        c=Path(d)/"seven_layer_menu.c"
        c.write_text(code,encoding="utf-8")
        for mode,flags in (("offline",[]),("netplay",["-DNESRECOMP_NET"])):
            exe=Path(d)/("menu_"+mode+(".exe" if os.name=="nt" else ""))
            subprocess.run([a.cc,"-std=c11","-O2","-Wall","-Wextra",
                            "-Werror","-pedantic",*flags,"-I",str(ENHANCED),str(c),
                            str(ENHANCED/"cviii_start_menu_0623.c"),
                            str(ENHANCED/"cviii_slot_picker_0625.c"),
                            str(ENHANCED/"cviii_menu_stack_0626.c"),
                            str(ENHANCED/"cviii_audio_menu_state_0622.c"),
                            str(ENHANCED/"cviii_audio_levels_0618.c"),
                            "-o",str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
    print("PASS JP0644 actual seventh-layer C11 menu shell offline/netplay")


if __name__=="__main__":
    main()
