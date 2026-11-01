#ifndef TEST_CYC_TRACE_H
#define TEST_CYC_TRACE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
 uint32_t index;uint16_t addr;uint8_t value,data_bus;
 char kind;bool dma,halt,done;
} CycTraceCycle;
extern bool cyc_trace_enabled;
extern void (*cyc_trace_cycle_hook)(const CycTraceCycle *);
#endif