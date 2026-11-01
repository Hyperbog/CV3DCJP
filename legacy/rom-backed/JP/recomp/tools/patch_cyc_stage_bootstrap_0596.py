#!/usr/bin/env python3
"""JP0596: add one-time password-screen bootstrap to the F10 checkpoint menu.

This patch reads the generated JP0595 window-host overlay and produces a
third overlay in the private external NESRecomp working tree. No upstream
source, game ROM, or compiled game instructions are committed.
"""
from __future__ import annotations

import argparse
from pathlib import Path

MARKER = "CVIII_PASSWORD_BOOTSTRAP_0596"

C_BOOTSTRAP = r'''
/* CVIII_PASSWORD_BOOTSTRAP_0596.
 * F6 captures a real original Japanese password-entry screen. If no
 * stage checkpoint exists, Enter restores that screen and fills the
 * 4x4 original-game password RAM for the selected stage/partner/mode.
 * The game's normal password submission is still required and is NOT
 * replaced with speculative PC/IRQ/mapper/RAM stage initialization.
 */
static void cviii_bootstrap_path(char *out, size_t n)
{
    char base[1200];
    state_slot(base, sizeof(base));
    char *dot = strrchr(base, '.');
    if (dot) *dot = 0;
    snprintf(out, n, "%s.debug.password-entry.cycstate", base);
}
static bool cviii_capture_password_screen(void)
{
    /* JP retail bank 03 uses $19=08 for its live password-entry state.
     * Refuse to capture arbitrary gameplay as a supposedly safe startup. */
    if (cyc_mod_peek(0x0019) != 0x08) {
        note("BOOTSTRAP REFUSED", "F6 requires original password-entry screen");
        return false;
    }
    char path[1400], err[256] = "";
    cviii_bootstrap_path(path, sizeof(path));
    bool ok = save_state_to(path, err, sizeof(err));
    note(ok ? "PASSWORD SCREEN SAVED" : "CAPTURE FAILED", ok ? NULL : err);
    return ok;
}
static bool cviii_password_combo_supported(unsigned stage, unsigned partner, bool hard)
{
    static const unsigned char allow[6] = { 0x07,0x03,0x2F,0xFF,0x00,0x3D };
    if (stage >= 18 || partner >= 4) return false;
    if (hard || stage >= 16 || partner == 0) return true;
    return (allow[(partner - 1) * 2 + (stage >> 3)] & (0x80 >> (stage & 7))) != 0;
}
static bool cviii_stage_password_bootstrap(void)
{
    unsigned save = cviii_stage_sel;
    unsigned partner = cviii_partner_ids[cviii_partner_sel];
    if (!cviii_password_combo_supported(save, partner, cviii_hard_mode)) {
        note("ROUTE DISALLOWED", "Choose another companion or TAB for Hard mode");
        return false;
    }
    char path[1400], err[256] = "";
    cviii_bootstrap_path(path, sizeof(path));
    /* Back up the complete machine. A wrong/corrupt password-screen save
     * or failed RAM write must not strand the current game session. */
    uint8_t *backup = NULL;
    size_t backup_size = 0;
    if (!cyc_state_save(&backup, &backup_size, err, sizeof(err))) {
        note("BOOTSTRAP BACKUP FAILED", err);
        return false;
    }
    bool loaded = load_state_from(path, err, sizeof(err));
    if (!loaded) {
        free(backup);
        note("NO PASSWORD SCREEN", "Visit Password once; F10 then F6 to capture");
        return false;
    }
    bool ok = cyc_mod_peek(0x0019) == 0x08;
    if (!ok) note("WRONG BOOTSTRAP STATE", "Password screen required");
    if (ok) {
        /* These small arrays are independently authenticated as exact
         * constant table bytes in original JP mapper24 PRG bank 03. */
        static const unsigned char seeds[8] = {7,3,1,6,2,4,5,0};
        static const unsigned char selectors[9] = {
            0x01,0x1B,0x02,0x35,0x19,0x03,0x37,0x1A,0x36
        };
        static const unsigned char chains[3][9] = {
            {0x00,0x33,0x20,0x13,0x22,0x01,0x11,0x03,0x32},
            {0x12,0x10,0x02,0x32,0x23,0x13,0x30,0x21,0x01},
            {0x31,0x13,0x01,0x22,0x10,0x30,0x33,0x03,0x21}
        };
        unsigned namehash = 0;
        for (unsigned i = 0; i < 8; ++i)
            namehash += cyc_mod_peek((uint16_t)(0x07F8 + i)) + seeds[i];
        namehash &= 7;
        unsigned payload = (namehash << 5) | ((save & 1) << 4)
                           | (partner << 1) | (cviii_hard_mode ? 1 : 0);
        /* A valid toggle-zero password. This does not set extra RAM fields. */
        unsigned mixed = payload ^ 0x55;
        unsigned hash = (save +
            (((((payload >> 4) + (payload & 15)) & 15) << 4)) +
            (((mixed >> 4) + (mixed & 15)) & 15)) & 255;
        unsigned selector = selectors[save >> 1];
        unsigned leader = (selector & 0x30) | ((selector >> 2) & 3);
        int chain = -1;
        for (int j = 0; j < 3; ++j)
            if (chains[j][0] == leader) chain = j;
        if (chain < 0) ok = false;
        unsigned char cells[16] = {0};
        if (ok) {
            for (int k = 0; k < 9; ++k) {
                unsigned pos = chains[chain][k];
                unsigned symbol = k == 0 ? (selector & 3) :
                    ((((payload >> (k - 1)) & 1) << 1) |
                     ((hash >> (k - 1)) & 1));
                /* $0790... are packed rrccmm values, NOT just mark IDs.
                 * Blanks must remain zero; each nonblank needs row/column. */
                cells[cviii_pwd_pos(pos)] = (unsigned char)
                    (symbol == 0 ? 0 :
                     ((pos & 0x30) | ((pos & 3) << 2) | symbol));
            }
            for (unsigned i = 0; i < 16; ++i) {
                if (!cyc_mod_poke((uint16_t)(0x0790 + i), cells[i])) {
                    ok = false;
                    break;
                }
            }
        }
    }
    if (!ok) {
        char restore_err[256] = "";
        bool restored = cyc_state_load(backup, backup_size,
                                       restore_err, sizeof(restore_err));
        if (restored) {
            s_frames_done = (long)cyc_ring_frame;
            cyc_session_state_loaded();
        }
        note(restored ? "BOOTSTRAP ROLLED BACK" : "RESTORE FAILED",
             restored ? "No game state modified" : restore_err);
    }
    free(backup);
    return ok;
}
'''

def once(source: str, needle: str, replacement: str) -> str:
    if source.count(needle) != 1:
        raise ValueError("Pinned JP0595 SDL anchor changed: " + repr(needle[:80]))
    return source.replace(needle, replacement, 1)


def generate(source: str) -> str:
    if MARKER in source or "CVIII_STAGE_WARP_0594" not in source or "cviii_pwd_decode" not in source:
        raise ValueError("Expected one JP0595 stage/password host overlay")
    source = once(source, "static bool cviii_stage_open;",
                  "static bool cviii_stage_open;\nstatic bool cviii_hard_mode;")
    source = once(source, "static bool cviii_stage_key(const SDL_KeyboardEvent *key)",
                  C_BOOTSTRAP + "\nstatic bool cviii_stage_key(const SDL_KeyboardEvent *key)")
    source = once(
        source,
        "    if (!cviii_stage_open) return false;\n    if (sc == SDL_SCANCODE_V",
        "    if (!cviii_stage_open) return false;\n"
        "    if (sc == SDL_SCANCODE_TAB) { cviii_hard_mode = !cviii_hard_mode; return true; }\n"
        "    if (sc == SDL_SCANCODE_F6) { cviii_capture_password_screen(); return true; }\n"
        "    if (sc == SDL_SCANCODE_V"
    )
    source = once(
        source,
        '''        bool ok = saving ? save_state_to(path, err, sizeof(err))
                         : load_state_from(path, err, sizeof(err));
        snprintf(cviii_stage_status, sizeof(cviii_stage_status), "%s",
                 ok ? (saving ? "CHECKPOINT CAPTURED" : "STAGE LOADED") :
                      (saving ? "CAPTURE FAILED" : "NO COMPATIBLE CHECKPOINT"));
        note(cviii_stage_status, ok ? NULL : err);
        if (ok && !saving) cviii_stage_open = false;''',
        '''        bool ok = saving ? save_state_to(path, err, sizeof(err))
                         : load_state_from(path, err, sizeof(err));
        bool prefilled = false;
        if (!saving && !ok) {
            prefilled = cviii_stage_password_bootstrap();
            ok = prefilled;
        }
        snprintf(cviii_stage_status, sizeof(cviii_stage_status), "%s",
                 ok ? (saving ? "CHECKPOINT CAPTURED" :
                       prefilled ? "PASSWORD PREFILLED" : "STAGE LOADED") :
                      (saving ? "CAPTURE FAILED" : "NO STAGE / BOOTSTRAP"));
        if (ok) note(cviii_stage_status,
                     prefilled ? "Press game Password confirm button to begin stage" : NULL);
        if (ok && !saving) cviii_stage_open = false;'''
    )
    source = once(
        source,
        '''    snprintf(out, size, "%s.debug.stage%02u.partner%u.cycstate", base,
             cviii_stage_sel, cviii_partner_ids[cviii_partner_sel]);''',
        '''    if (cviii_hard_mode)
        snprintf(out, size, "%s.debug.stage%02u.partner%u.hard.cycstate", base,
                 cviii_stage_sel, cviii_partner_ids[cviii_partner_sel]);
    else
        snprintf(out, size, "%s.debug.stage%02u.partner%u.cycstate", base,
                 cviii_stage_sel, cviii_partner_ids[cviii_partner_sel]);'''
    )
    source = once(
        source,
        '''    cviii_stage_print(14, 160, partner);
    cviii_stage_print(14, 177, "ENTER = WARP TO SAVED CHECKPOINT");
    cviii_stage_print(14, 188, "F5 = CAPTURE CURRENT GAME HERE");
    cviii_stage_print(14, 199, "CTRL+V = SELECT FROM PASSWORD");
    cviii_stage_print(14, 210, "ESC = CLOSE / KEEP PLAYING");
    SDL_SetRenderDrawColor(s_ren, 238, 190, 88, 255);
    cviii_stage_print(14, 222, cviii_stage_status);''',
        '''    cviii_stage_print(14, 160, partner);
    cviii_stage_print(14, 171, cviii_hard_mode ? "MODE: HARD (TAB TO CHANGE)" :
                      "MODE: NORMAL (TAB TO CHANGE)");
    cviii_stage_print(14, 183, "ENTER = CHECKPOINT OR PASSWORD");
    cviii_stage_print(14, 194, "F6 = SAVE PASSWORD SCREEN ONCE");
    cviii_stage_print(14, 205, "F5 = CAPTURE STAGE CTRL+V = PASTE");
    SDL_SetRenderDrawColor(s_ren, 238, 190, 88, 255);
    cviii_stage_print(14, 220, cviii_stage_status);'''
    )
    return source


def smoke() -> None:
    stub = "\n".join((
        "/* CVIII_STAGE_WARP_0594 */", "/* cviii_pwd_decode */",
        "static bool cviii_stage_open;",
        "static bool cviii_stage_key(const SDL_KeyboardEvent *key)",
        "    if (!cviii_stage_open) return false;",
        "    if (sc == SDL_SCANCODE_V",
        '        bool ok = saving ? save_state_to(path, err, sizeof(err))',
        '                         : load_state_from(path, err, sizeof(err));',
        '        snprintf(cviii_stage_status, sizeof(cviii_stage_status), "%s",',
        '                 ok ? (saving ? "CHECKPOINT CAPTURED" : "STAGE LOADED") :',
        '                      (saving ? "CAPTURE FAILED" : "NO COMPATIBLE CHECKPOINT"));',
        '        note(cviii_stage_status, ok ? NULL : err);',
        '        if (ok && !saving) cviii_stage_open = false;',
        '    snprintf(out, size, "%s.debug.stage%02u.partner%u.cycstate", base,',
        '             cviii_stage_sel, cviii_partner_ids[cviii_partner_sel]);',
        '    cviii_stage_print(14, 160, partner);',
        '    cviii_stage_print(14, 177, "ENTER = WARP TO SAVED CHECKPOINT");',
        '    cviii_stage_print(14, 188, "F5 = CAPTURE CURRENT GAME HERE");',
        '    cviii_stage_print(14, 199, "CTRL+V = SELECT FROM PASSWORD");',
        '    cviii_stage_print(14, 210, "ESC = CLOSE / KEEP PLAYING");',
        '    SDL_SetRenderDrawColor(s_ren, 238, 190, 88, 255);',
        '    cviii_stage_print(14, 222, cviii_stage_status);'
    ))
    patched = generate(stub)
    assert patched.count(MARKER) == 1
    assert "0x0790 + i" in patched and "0x07F8 + i" in patched
    assert "cyc_state_load(backup" in patched
    assert "cviii_hard_mode = !cviii_hard_mode" in patched
    try:
        generate(patched)
    except ValueError:
        pass
    else:
        raise AssertionError("Applied JP0596 overlay twice")
    print("PASS JP0596 password-entry bootstrap F10 overlay contract")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", type=Path)
    p.add_argument("--out", type=Path)
    p.add_argument("--smoke", action="store_true")
    args = p.parse_args()
    if args.smoke:
        smoke()
        return
    if not args.source or not args.out:
        p.error("--source and --out required")
    src = args.source.resolve(strict=True)
    out = args.out.resolve()
    if (src.name != "cviii_cyc_sdl_debug.c" or
        out.name != "cviii_cyc_sdl_stage_bootstrap.c" or
        src.parent != out.parent):
        raise ValueError("JP0596 patch must follow JP0595 in external SDL directory")
    out.write_text(generate(src.read_text(encoding="utf-8")), encoding="utf-8")
    print("PASS JP0596 native password-entry overlay: " + str(out))


if __name__ == "__main__":
    main()
