/* JP0649: bounded original DMC parameter/sample-region and raw bit decoder.
 * Read-only host research; does NOT replace NES APU, DMA or game audio.
 * Physical bank8 is an EXPLICIT observed VRC6 $C000 register snapshot.
 */
#ifndef CVIII_DPCM_DECODE_0649_H
#define CVIII_DPCM_DECODE_0649_H
#include "cviii_dmc_source_0648.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CVIII_DPCM_PRG_SIZE (256u * 1024u)
#define CVIII_DPCM_BANK_SIZE 8192u
#define CVIII_DPCM_MAX_SAMPLE_BYTES 4081u
#define CVIII_DPCM_MAX_BITS (CVIII_DPCM_MAX_SAMPLE_BYTES * 8u)
typedef enum {CVIII_DPCM_NTSC=0, CVIII_DPCM_PAL=1} CviiiDpcmRegion;
typedef struct {
    uint8_t command, index, bank8, initial_level, rate_index;
    bool loop, irq_enabled;
    uint16_t cpu_start, cpu_end, period_cpu_cycles;
    uint32_t physical_start, byte_count, bit_count;
    uint64_t bit_period_total_cycles; /* idealized full-sample bit periods */
} CviiiDpcmSpan;
/* Accept only a successful, internally consistent JP0648 register plan,
 * and a caller-observed 0..31 bank8 mapper snapshot. All 15 current
 * original-JP sample spans fit $C000..$DFFF; abort on crossing instead
 * of silently choosing an unobserved mapper bank. No out modification
 * on failure; no ROM bytes are embedded in this module. */
bool cviii_dpcm_span_0649(const CviiiDmcPlan *plan, unsigned bank8,
                          CviiiDpcmRegion region, CviiiDpcmSpan *out);
/* Mathematical LSB-first 7-bit DPCM delta sequence for ONE sample pass.
 * out[i] is the DAC level after consuming bit i; clamp to 0..127 without
 * wrapping. A looped descriptor gets one iteration ONLY; caller handles
 * hardware loop, buffering, IRQ, DMA and exact initial timer phase.
 * Unlike NES hardware this does not emulate the sample-buffer pipeline.
 * 'prg' is the owner/private 256KiB cartridge PRG and never modified.
 * Requires exact 262144-byte ROM PRG and output space for all bits.
 * On failure output and final_level are unmodified. */
bool cviii_dpcm_bits_0649(const uint8_t *prg, size_t prg_size,
                          const CviiiDpcmSpan *span,
                          uint8_t *output, size_t capacity,
                          uint8_t *final_level);
#endif
