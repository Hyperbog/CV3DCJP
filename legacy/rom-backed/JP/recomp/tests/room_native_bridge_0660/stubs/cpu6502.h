#ifndef TEST_CPU6502_H
#define TEST_CPU6502_H
#include <stdint.h>
typedef struct {uint16_t pc;uint8_t a,x,y,s,c,z,n,v,i,d,do_nmi,do_irq,do_reset;} Cpu6502;
extern Cpu6502 cpu;
extern void (*cyc_cpu_rts_observer)(void);
extern uint8_t (*cyc_cpu_ram_read_policy)(uint16_t,uint16_t,uint8_t);
#endif