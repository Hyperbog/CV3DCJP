/* Phase 5G-2: ROM-free, presentation-only PPU margin qualification.
 * Adapted from CVIII draft PR #13's JP0830 verification strategy.
 * A passing gate is internal NESRecomp consistency, NOT independent Mesen parity.
 */
#ifndef CVIII_PPU_GUARD_5G2_H
#define CVIII_PPU_GUARD_5G2_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint8_t (*nametable)(uint16_t);
    uint8_t (*chr)(uint16_t);
    uint32_t (*color)(int);
    int (*scroll_x)(int);
    int (*scroll_y)(int);
    uint16_t (*bg_table)(int);
    uint8_t (*mask)(int);
    const uint8_t *oam;
} CviiiPpuSource5G2;
/* Returns a stable failure reason and zero output writes on failure.
 * Only margin columns, and only rows 48..239, may change on success. */
typedef enum {
    CVIII_PPU_OK=0, CVIII_PPU_BAD_INPUT, CVIII_PPU_BAD_LINES,
    CVIII_PPU_MIRRORED, CVIII_PPU_CENTER_MISMATCH,
    CVIII_PPU_NO_GEOMETRY
} CviiiPpuResult5G2;
CviiiPpuResult5G2 cviii_ppu_verified_paint_5g2(
    const CviiiPpuSource5G2 *src, const uint32_t *original,
    uint32_t *canvas, int width, int height, int native_x,
    uint32_t *painted);
const char *cviii_ppu_result_name_5g2(CviiiPpuResult5G2 reason);
#endif
