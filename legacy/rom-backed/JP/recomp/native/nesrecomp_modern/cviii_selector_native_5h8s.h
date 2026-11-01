#ifndef CVIII_SELECTOR_NATIVE_5H8S_H
#define CVIII_SELECTOR_NATIVE_5H8S_H
#include <stdint.h>
/* Only the opt-in CVIII modern host calls this; no production world writes. */
void cviii_selector_trace_power_on_5h8s(void);
/* Defined in the instrumented cycle machine. Fields are sampled before RAM store. */
extern void (*cyc_cviii_selector_write_5h8s)(uint16_t fetched_pc,
    int32_t prg_offset, uint16_t address, uint8_t old_value,
    uint8_t new_value, uint8_t stage, uint8_t area, uint8_t room);
#endif
