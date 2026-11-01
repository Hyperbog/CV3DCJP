#ifndef TEST_CYC_CORE_H
#define TEST_CYC_CORE_H
#include <stdint.h>
#include <stdbool.h>
const uint8_t *cyc_cpu_ram(void);
bool cyc_debug_peek(uint16_t addr,uint8_t *v);
uint64_t cyc_cycle_count(void);
#endif