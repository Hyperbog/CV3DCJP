#!/usr/bin/env python3
"""JP0625: extend owner-private JP0623 Pause SDL overlay with Save/Load slot picker.

Sixth layer uses the existing verified host quick-save slot variable
and full-machine state handlers. No ROM data or upstream sources committed.
"""
from __future__ import annotations
import argparse
from pathlib import Path

MARKER="CVIII_SLOT_SELECTOR_0625"
START_ACTION="static void cviii_pause_action(int action)\n{"
SAVE='''    case CVIII_START_SAVE:
        (void)save_state_slot();
        break;'''
LOAD='''    case CVIII_START_LOAD:
        (void)load_state_slot();
        break;'''
KEY='''    if(!cviii_pause_active()) return false;
    if(sc==SDL_SCANCODE_ESCAPE || sc==SDL_SCANCODE_BACKSPACE) {'''
PAD='''    if(!cviii_pause_active())return;
    switch(button) {'''
HELPERS=r'''
/* CVIII_SLOT_SELECTOR_0625: Pause is already open while selecting a slot.
 * Only the original host changes cviii_save_slot and serializes machine state. */
static void cviii_pause_slot_commit(void)
{
    unsigned selected=0u;
    const int action=cviii_pause_slot_accept(&selected);
    if(action!=CVIII_START_SAVE && action!=CVIII_START_LOAD)return;
    /* The picker accepts first; I/O must not permanently change the
     * active quick slot until the original host confirms success. */
    const unsigned previous=cviii_save_slot;
    cviii_save_slot=selected;
    const bool ok=action==CVIII_START_SAVE ?
        save_state_slot() : load_state_slot();
    if(!ok) {
        cviii_save_slot=previous;
        /* Keep the chosen slot displayed and the game paused for retry
         * or cancellation; the native host already reports I/O errors. */
        if(!cviii_pause_slot_begin(action,selected))
            note("SAVE SLOT", "Could not restore slot picker");
        return;
    }
    cviii_pause_close();
}
'''
SLOT_KEY=r'''    if(cviii_pause_slot_active()) {
        if(sc==SDL_SCANCODE_ESCAPE || sc==SDL_SCANCODE_BACKSPACE) {
            if(!key->repeat)cviii_pause_slot_cancel();
            return true;
        }
        if(sc==SDL_SCANCODE_UP || sc==SDL_SCANCODE_LEFT ||
           sc==SDL_SCANCODE_DOWN || sc==SDL_SCANCODE_RIGHT) {
            if(!key->repeat)
                (void)cviii_pause_slot_step(
                    (sc==SDL_SCANCODE_UP || sc==SDL_SCANCODE_LEFT)?-1:1);
            return true;
        }
        if(sc==SDL_SCANCODE_RETURN || sc==SDL_SCANCODE_KP_ENTER ||
           sc==SDL_SCANCODE_SPACE) {
            if(!key->repeat)cviii_pause_slot_commit();
            return true;
        }
        return true; /* modal input never leaks into emulated buttons */
    }
'''
SLOT_PAD=r'''    if(cviii_pause_slot_active()) {
        switch(button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            (void)cviii_pause_slot_step(-1);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            (void)cviii_pause_slot_step(1);
            break;
        case SDL_CONTROLLER_BUTTON_A:
            cviii_pause_slot_commit();
            break;
        case SDL_CONTROLLER_BUTTON_B:
            cviii_pause_slot_cancel(); /* return to Start/Pause */
            break;
        case SDL_CONTROLLER_BUTTON_START:
            cviii_pause_close();
            cviii_audio_menu_toggled=true;
            break;
        default:break;
        }
        return;
    }
'''
def once(source:str,old:str,new:str)->str:
    if source.count(old)!=1:
        raise ValueError("JP0625 pinned owner-private SDL anchor changed: "+repr(old[:95]))
    return source.replace(old,new,1)

def generate(source:str)->str:
    if MARKER in source or source.count("CVIII_START_PAUSE_0623")!=1:
        raise ValueError("JP0625 requires unmodified JP0623 5-layer SDL source")
    source=once(source,START_ACTION,HELPERS+"\n"+START_ACTION)
    source=once(source,SAVE,'''    case CVIII_START_SAVE:
        if(!cviii_pause_slot_begin(action,cviii_save_slot))
            note("SAVE SLOT", "Could not begin slot selection");
        break;''')
    source=once(source,LOAD,'''    case CVIII_START_LOAD:
        if(!cviii_pause_slot_begin(action,cviii_save_slot))
            note("LOAD SLOT", "Could not begin slot selection");
        break;''')
    source=once(source,KEY,'''    if(!cviii_pause_active()) return false;
'''+SLOT_KEY+'''    if(sc==SDL_SCANCODE_ESCAPE || sc==SDL_SCANCODE_BACKSPACE) {''')
    source=once(source,PAD,'''    if(!cviii_pause_active())return;
'''+SLOT_PAD+'''    switch(button) {''')
    return source

def smoke()->None:
    synthetic="\n".join((
        "/* CVIII_START_PAUSE_0623 */",
        "static unsigned cviii_save_slot;",
        START_ACTION,SAVE,LOAD,
        "static bool cviii_pause_key(const SDL_KeyboardEvent *key) {",
        KEY,"}",
        "static void cviii_pause_button(int button) {",PAD,"}"
    ))
    result=generate(synthetic)
    assert result.count(MARKER)==1
    assert result.count("cviii_pause_slot_commit();")==2
    assert "cviii_save_slot=selected;" in result
    assert "save_state_slot() : load_state_slot()" in result
    assert "SDL_CONTROLLER_BUTTON_DPAD_RIGHT" in result
    assert "cviii_pause_slot_cancel();" in result
    assert "cviii_save_slot=previous;" in result
    assert "cviii_pause_slot_begin(action,selected)" in result
    assert "cviii_pause_close();" in result
    for bad in (result,synthetic.replace(SAVE,"/* missing save action */")):
        try:generate(bad)
        except ValueError:continue
        raise AssertionError("Double-patched or drifted SDL accepted")
    print("PASS JP0625 six-stage original-host Save/Load slot source fixtures")

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--smoke",action="store_true")
    p.add_argument("--source",type=Path)
    p.add_argument("--out",type=Path)
    args=p.parse_args()
    if args.smoke:smoke();return
    if not args.source or not args.out:p.error("--source and --out required")
    source=args.source.resolve(strict=True);out=args.out.resolve()
    if source.parent!=out.parent or source==out or \
       source.name!="cviii_cyc_sdl_start_menu.c" or \
       out.name!="cviii_cyc_sdl_slot_picker.c":
        raise ValueError("JP0625 requires owner-private adjacent SDL stage files")
    out.write_text(generate(source.read_text(encoding="utf-8")),encoding="utf-8")
    print("PASS JP0625 owner-private SDL slot picker overlay: "+str(out))

if __name__=="__main__":main()
