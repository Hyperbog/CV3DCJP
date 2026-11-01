#ifndef CVIII_SELECTOR_NATIVE_5H8U_H
#define CVIII_SELECTOR_NATIVE_5H8U_H
#include <stdint.h>
/* 21 observation fields. No pointer retained past the synchronous callback. */
void cviii_selector_trace_power_on_5h8u(void);
extern void (*cyc_cviii_selector_write_5h8u)(const uint64_t *fields);
#endif
