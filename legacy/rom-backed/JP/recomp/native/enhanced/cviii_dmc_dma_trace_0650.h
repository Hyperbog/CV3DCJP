/* JP0650: owner-opt-in actual mapper24 DMC-DMA fetch and PRG-bank trace.
 * Read-only host evidence. Never affects guest CPU/APU/mapper or PCM.
 * CSV may contain ROM-derived sample bytes: keep owner-local, never commit.
 */
#ifndef CVIII_DMC_DMA_TRACE_0650_H
#define CVIII_DMC_DMA_TRACE_0650_H
#include <stdint.h>
#include <stdio.h>
typedef struct {
    FILE *output;
    uint64_t rows, fetches, bank_requests, faults, dropped, last_cycle;
    uint32_t epoch;
    unsigned seen_cycle;
} CviiiDmcDmaTrace;
int cviii_dmc_dma_trace_open(CviiiDmcDmaTrace *trace, const char *path);
void cviii_dmc_dma_trace_close(CviiiDmcDmaTrace *trace);
/* Called after actual hw_bus_read, using PRG offset sampled BEFORE read.
 * 'physical' is upstream hw_cart.prg_off[4K slot] | (addr & $FFF).
 * bytes_before is APU dmc_bytes before this fetch's decrement. */
void cviii_dmc_dma_trace_fetch(uint64_t cycle, uint16_t addr,
                               uint32_t physical, uint8_t read_value,
                               unsigned bytes_before);
/* Observe CPU's mapper24 $Cxxx bank-register write intent, BEFORE
 * upstream vrc6_write commits mapping. This is not installed-bank proof. */
void cviii_dmc_dma_trace_bank_write(uint64_t cycle, uint16_t addr,
                                    uint8_t value);
/* Power-on/load separate observation epochs. Captures never serialized. */
void cviii_dmc_dma_trace_boundary(const char *kind, uint64_t cycle);
#endif
