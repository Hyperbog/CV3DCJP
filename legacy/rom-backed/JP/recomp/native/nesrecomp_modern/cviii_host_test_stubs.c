/* Synthetic frame/PPU adapters only for CVIII modern host unit tests.
 * NEVER link these stubs to the real game executable. */
#include "cyc_core.h"
#include "cyc_render.h"
#include <string.h>
static uint32_t game_frame[256 * 240];
static uint8_t bg_opaque[256 * 240];
static uint8_t memory[2048];
static uint8_t oam_bytes[256];
const uint32_t *cyc_frame_argb(void) { return game_frame; }
const uint8_t *cyc_frame_bg_opaque(void) { return bg_opaque; }
const uint8_t *cyc_cpu_ram(void) { return memory; }
void cyc_render_capture_background(bool enabled) { (void)enabled; }
bool cyc_render_background(uint32_t *out) {
    if (!out) return false;
    memset(out, 0, sizeof(game_frame));
    return true;
}
const uint8_t *cyc_render_oam(void) { return oam_bytes; }
uint8_t cyc_render_line_mask(int y) { (void)y; return 0x1c; }
bool cyc_render_line_sprite16(int y) { (void)y; return false; }
uint16_t cyc_render_line_sprite_table(int y) { (void)y; return 0; }
uint8_t cyc_render_chr(uint16_t address) { return (address>=16u && address<24u) ? 0xffu : 0; }
uint32_t cyc_render_color(int entry) { return UINT32_C(0xff000000) | (uint32_t)entry; }
void cyc_render_sprites(uint32_t *out, int width, int height, int native_x0,
                        const uint8_t *bg, CycSpritePlaceFn place, void *user)
{
    (void)out; (void)width; (void)height; (void)native_x0;
    (void)bg; (void)place; (void)user;
}

/* Deterministic 5G synthetic nametable: page 0 -> tile 1, page 1 -> tile 2. */
int cyc_render_line_scroll_x(int y) { (void)y; return 32; }
int cyc_render_line_scroll_y(int y) { return y; }
uint16_t cyc_render_line_bg_table(int y) { (void)y; return 0; }
uint8_t cyc_render_nametable(uint16_t addr) {
    unsigned local=addr&0x3ffu;
    return local<0x3c0u ? (uint8_t)(((addr>>10u)&1u)+1u) : 0;
}

/* 5H-8C read-only scene-epoch hardware observations for synthetic tests. */
static uint8_t scene_test_ciram[2048];
static uint8_t scene_test_palette[32];
const uint8_t *cyc_ppu_ciram(size_t *len) {
    if (len) *len=sizeof scene_test_ciram;
    return scene_test_ciram;
}
const uint8_t *cyc_ppu_palette(void) { return scene_test_palette; }
