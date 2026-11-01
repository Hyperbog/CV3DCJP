#!/usr/bin/env python3
"""JP 0591: private-build 10-slot SDL host overlay for Original and Enhanced.

Patch ONLY the separately obtained, pinned NESRecomp SDL host, writing the
derived C file next to the external original (to preserve relative includes).
Neither upstream source nor a game ROM is checked into the CVIII repository.

Slot 0 retains the pinned SDL host's existing unnumbered save filename.
Ctrl+0..9 selects slot; F8 saves, F9 loads. Both editions use identical
cyc_state_save_file/cyc_state_load_file whole-machine serialization.
"""
from __future__ import annotations
import argparse
from pathlib import Path

MARKER = "CVIII_SAVE_SLOTS_0591"
DECLARE = "static void state_slot(char *out, size_t n)\n{"
PATH_LINE = '    snprintf(out, n, "%s/%s.%s", dir, stem, s_fds ? "state" : "cycstate");'
EVENT = "            else if (ev.type == SDL_KEYDOWN) {\n#ifdef CYC_DEV_UI"
SAVE_TITLE = '    note(ok ? "STATE SAVED" : "STATE NOT SAVED", ok ? NULL : err);'
LOAD_TITLE = '    note(ok ? "STATE LOADED" : "STATE NOT LOADED", ok ? NULL : err);'

def once(src: str, original: str, replacement: str) -> str:
    if src.count(original) != 1:
        raise ValueError("pinned NESRecomp host source changed: expected one anchor")
    return src.replace(original, replacement, 1)

def generate(src: str) -> str:
    if MARKER in src:
        raise ValueError("already slot-patched source provided; start from upstream")
    src = once(src, DECLARE,
        "/* " + MARKER + ": selector affects host filenames only, not machine state. */\n"
        "static unsigned cviii_save_slot; /* 0 keeps existing Alpha filename. */\n"
        + DECLARE)
    src = once(src, PATH_LINE,
        '    if (!cviii_save_slot)\n'
        + PATH_LINE + '\n'
        '    else snprintf(out, n, "%s/%s.slot%u.%s", dir, stem,\n'
        '                  cviii_save_slot, s_fds ? "state" : "cycstate");')
    src = once(src, SAVE_TITLE,
        '    char title[40];\n'
        '    snprintf(title, sizeof(title), "SLOT %u SAVED", cviii_save_slot);\n'
        '    note(ok ? title : "STATE NOT SAVED", ok ? NULL : err);')
    src = once(src, LOAD_TITLE,
        '    char title[40];\n'
        '    snprintf(title, sizeof(title), "SLOT %u LOADED", cviii_save_slot);\n'
        '    note(ok ? title : "STATE NOT LOADED", ok ? NULL : err);')
    src = once(src, EVENT,
        """            else if (ev.type == SDL_KEYDOWN) {
                /* CVIII_SAVE_SLOTS_0591: Ctrl+0..9 selects a quick-save slot.
                 * Do not consume F8/F9; keep upstream rebinding and netplay
                 * save restrictions. Never select while the menu is open.
                 */
                if (!ev.key.repeat && !open &&
                    (ev.key.keysym.mod & KMOD_CTRL)) {
                    SDL_Scancode key = ev.key.keysym.scancode;
                    int slot = -1;
                    if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_9)
                        slot = (int)(key - SDL_SCANCODE_1) + 1;
                    else if (key == SDL_SCANCODE_0)
                        slot = 0;
                    if (slot >= 0) {
                        cviii_save_slot = (unsigned)slot;
                        char msg[48];
                        snprintf(msg, sizeof(msg), "Selected slot %u (F8 save / F9 load)",
                                 cviii_save_slot);
                        note("SAVE SLOT", msg);
                        continue;
                    }
                }
#ifdef CYC_DEV_UI""")
    return src

def validate(src: str, patched: str) -> None:
    for marker in (
        "static unsigned cviii_save_slot;",
        "if (!cviii_save_slot)",
        "saves",  # upstream code still chooses its original save directory
        '.slot%u.%s"',
        "SDL_SCANCODE_0",
        "SDL_SCANCODE_9",
        "note(ok ? title : ",
        "cyc_state_save_file(path, err, n);",
        "cyc_state_load_file(path, err, n)",
        "s_frames_done = (long)cyc_ring_frame;",
        "cyc_session_state_loaded();",
    ):
        if marker not in patched:
            raise ValueError("generated host overlay lost required save/load field: " + marker)
    if patched.count(MARKER) < 2:
        raise ValueError("slot patch markers absent")

def patch(source: Path, output: Path) -> None:
    source = source.resolve(strict=True)
    output = output.resolve()
    if source == output or source.parent != output.parent:
        raise ValueError("overlay must be beside, but not replace, external cyc_sdl.c")
    if source.name != "cyc_sdl.c" or output.name != "cviii_cyc_sdl_slots.c":
        raise ValueError("unexpected external SDL host source/overlay filenames")
    src = source.read_text(encoding="utf-8")
    updated = generate(src)
    validate(src, updated)
    output.write_text(updated, encoding="utf-8")
    print("PASS CVIII multi-slot SDL overlay, original upstream file unchanged: " + str(output))

def smoke() -> None:
    # Contract fixture, not copyrighted ROM, and not an external SDL compilation.
    stub = "\n".join([
        DECLARE,
        "    char stem[512], dir[1100];",
        PATH_LINE,
        "}",
        SAVE_TITLE,
        LOAD_TITLE,
        "    if (!cyc_state_load_file(path, err, n)) return false;",
        "    s_frames_done = (long)cyc_ring_frame;",
        "    cyc_session_state_loaded();",
        "    return cyc_state_save_file(path, err, n);",
        "    exe_path(dir, sizeof(dir), \"saves\");",
        EVENT,
        "    ",
    ])
    generated = generate(stub)
    validate(stub, generated)
    assert PATH_LINE in generated  # Alpha legacy slot 0 preserved
    assert "cviii_save_slot = (unsigned)slot;" in generated
    assert 'cviii_save_slot, s_fds ? "state" : "cycstate");' in generated
    try:
        generate(generated)
    except ValueError:
        pass
    else:
        raise AssertionError("double source-patching must fail")
    try:
        generate(stub.replace(PATH_LINE, "    /* upstream drift */"))
    except ValueError:
        pass
    else:
        raise AssertionError("missing pinned upstream anchor accepted")
    print("PASS JP0591 Original/Enhanced shared slot0..9 host overlay and pinned-anchor rejection")

def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", type=Path)
    p.add_argument("--out", type=Path)
    p.add_argument("--smoke", action="store_true")
    a = p.parse_args()
    if a.smoke:
        smoke()
        return
    if not a.source or not a.out:
        p.error("provide both --source and --out, or --smoke")
    patch(a.source, a.out)

if __name__ == "__main__":
    main()
