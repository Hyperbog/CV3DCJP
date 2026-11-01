#!/usr/bin/env python3
"""JP0626: Audio selected within Start/Pause must return to its parent.

Seventh owner-private SDL overlay, after JP0625 slot picker; never patches
upstream NESRecomp or original ROM bytes. Guard exact source anchors.
"""
from __future__ import annotations
import argparse
from pathlib import Path

MARKER="CVIII_AUDIO_RETURN_0626"
OLD_ACTION='''    case CVIII_START_AUDIO:
        cviii_pause_close();
        if (!cviii_audio_menu_toggle())
            note("AUDIO", "Original CHR assets unavailable");
        cviii_audio_menu_toggled=true;
        break;'''
NEW_ACTION='''    case CVIII_START_AUDIO:
        /* Parent remains resident. Audio alone owns input/presentation. */
        if (!cviii_pause_audio_enter())
            note("AUDIO", "Original CHR assets unavailable");
        cviii_audio_menu_toggled=true;
        break;'''
OLD_AUDIO_RECOMPUTE='''                    open=menu_open() || cviii_stage_open || cviii_audio_menu_active();
                    continue;'''
NEW_AUDIO_RECOMPUTE='''                    /* F6/Esc returning to Pause must never release game input
                     * even transiently inside the same SDL event batch. */
                    open=menu_open() || cviii_stage_open || cviii_audio_menu_active() ||
                         cviii_pause_active(); /* CVIII_AUDIO_RETURN_0626 */
                    continue;'''

def once(source:str,old:str,new:str)->str:
    count=source.count(old)
    if count!=1:
        raise ValueError("JP0626 pinned SDL source changed: "+repr(old[:110])+
                         ", count "+str(count))
    return source.replace(old,new,1)

def generate(source:str)->str:
    if MARKER in source or source.count("CVIII_SLOT_SELECTOR_0625")!=1:
        raise ValueError("JP0626 requires unmodified JP0625 six-layer SDL source")
    source=once(source,OLD_ACTION,NEW_ACTION)
    source=once(source,OLD_AUDIO_RECOMPUTE,NEW_AUDIO_RECOMPUTE)
    if "cviii_pause_audio_enter()" not in source:
        raise ValueError("Missing nested Audio owner API")
    return source

def smoke()->None:
    sample="\n".join((
        "/* CVIII_SLOT_SELECTOR_0625 */",
        "static void cviii_pause_action(int action) {",
        OLD_ACTION,"}",
        "static void cviii_audio_event(void) {",OLD_AUDIO_RECOMPUTE,"}"
    ))
    output=generate(sample)
    assert output.count(MARKER)==1
    assert output.count("cviii_pause_audio_enter()")==1
    assert "cviii_pause_close();" not in output
    assert "open=menu_open() || cviii_stage_open || cviii_audio_menu_active() ||\n                         cviii_pause_active();" in output
    for changed in (output,
                    sample.replace(OLD_ACTION,"/* old action missing */"),
                    sample.replace(OLD_AUDIO_RECOMPUTE,
                                   "/* old event recompute missing */")):
        try:generate(changed)
        except ValueError:continue
        raise AssertionError("JP0626 accepted a changed/double-patched SDL host")
    print("PASS JP0626 nested Audio<->Pause SDL return and event-batch pause gate")

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--smoke",action="store_true")
    p.add_argument("--source",type=Path)
    p.add_argument("--out",type=Path)
    a=p.parse_args()
    if a.smoke:smoke();return
    if not a.source or not a.out:p.error("--source and --out required")
    inp=a.source.resolve(strict=True);out=a.out.resolve()
    if inp.parent!=out.parent or inp==out or \
       inp.name!="cviii_cyc_sdl_slot_picker.c" or \
       out.name!="cviii_cyc_sdl_audio_return.c":
        raise ValueError("JP0626 accepts only adjacent private generated SDL layers")
    out.write_text(generate(inp.read_text(encoding="utf-8")),encoding="utf-8")
    print("PASS JP0626 final nested Audio SDL overlay: "+str(out))

if __name__=="__main__":main()
