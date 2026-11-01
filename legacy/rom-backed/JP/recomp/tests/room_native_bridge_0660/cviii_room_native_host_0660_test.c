#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_room_native_host_0660.h"
#include "cpu6502.h"
#include "cyc_core.h"
#include "cyc_trace.h"
#include "hw.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Cpu6502 cpu;
void (*cyc_cpu_rts_observer)(void);
uint8_t (*cyc_cpu_ram_read_policy)(uint16_t,uint16_t,uint8_t);
bool cyc_trace_enabled;
void (*cyc_trace_cycle_hook)(const CycTraceCycle *);
static unsigned bank4=0x2bu;
static uint8_t ram[2048],bytecode[3]={0xad,0xa0,0x07};
static uint64_t tick=100u;
const uint8_t *cyc_cpu_ram(void) {return ram;}
uint64_t cyc_cycle_count(void) {return tick;}
unsigned hw_prg_bank4(uint16_t addr) { (void)addr;return bank4; }
bool cyc_debug_peek(uint16_t addr,uint8_t *out) {
 if(!out || addr<0xb7f7 || addr>0xb7f9)return false;
 *out=bytecode[addr-0xb7f7];return true;
}
static CviiiRoomNativeBridge0660 bridge;
static void source(void) {
 static const unsigned roots[]={0xbea4u,0xbedbu,0xbf03u,0xbe8bu,0xbf1cu};
 static const unsigned counts[]={10u,7u,4u,4u,5u};
 bridge.ready=1;
 for(unsigned k=0;k<5;k++){
  unsigned at=0x38c2u+2*k,off=roots[k]-0x8000u;
  bridge.bank0a[at]=(uint8_t)roots[k];
  bridge.bank0a[at+1]=(uint8_t)(roots[k]>>8);
  for(unsigned j=0;j<4;j++)bridge.bank0e[off+j]=(uint8_t)(k+j+1);
  for(unsigned j=0;j<counts[k]*5;j++)bridge.bank0e[off+4+j]=(uint8_t)(j+k+1);
  bridge.bank0e[off+4+5*counts[k]]=0xffu;
 }
}
int main(void) {
 source();
 assert(cviii_bridge0660_attach(&bridge));
 assert(cyc_cpu_ram_read_policy && cyc_cpu_rts_observer && cyc_trace_cycle_hook);
 ram[0x23]=10;ram[0x7a0]=0;
 cpu.s=0xfc;cpu.i=1;cpu.d=0;cpu.v=1;
 ram[0x1fd]=0x34;ram[0x1fe]=0x12;
 cyc_trace_enabled=true;
 bank4=0x2a;
 assert(cyc_cpu_ram_read_policy(0xb7f7,0x7a0,ram[0x7a0])==0);
 assert(!bridge.active);
 bank4=0x2b;
 assert(cyc_cpu_ram_read_policy(0xb7f7,0x200,0x77)==0x77);
 bytecode[0]=0x00;
 assert(cyc_cpu_ram_read_policy(0xb7f7,0x7a0,ram[0x7a0])==0);
 assert(!bridge.active);
 bytecode[0]=0xad;
 cpu.do_irq=1;
 assert(cyc_cpu_ram_read_policy(0xb7f7,0x7a0,ram[0x7a0])==0);
 assert(!bridge.active);
 cpu.do_irq=0;
 ram[0x23]=0x0b;
 assert(cyc_cpu_ram_read_policy(0xb7f7,0x7a0,ram[0x7a0])==0);
 assert(!bridge.active);
 ram[0x23]=0x0a;
 ram[0x7a0]=0xff;
 assert(cyc_cpu_ram_read_policy(0xb7f7,0x7a0,ram[0x7a0])==0xff);
 assert(!bridge.active && bridge.skipped==1);
 ram[0x7a0]=0;
 assert(cyc_cpu_ram_read_policy(0xb7f7,0x7a0,ram[0x7a0])==0);
 assert(bridge.active && bridge.observed_entries==1);
 CviiiRoomNativeHostDiagnostics0660 stats={0};
 cviii_bridge0660_diagnostics(&stats);
 assert(stats.operand_reads==7 && stats.site_reads==6);
 assert(stats.wrong_bank==1 && stats.wrong_opcode==1);
 assert(stats.pending_interrupt==1 && stats.wrong_context==1);
 cpu.pc=0x9999;
 cyc_cpu_rts_observer();
 cviii_bridge0660_diagnostics(&stats);
 assert(stats.rts_while_active==1 && stats.unmatched_rts==1);
 assert(bridge.active);
 for(unsigned i=0;i<bridge.predicted.mapper_events;i++) {
  CycTraceCycle ev={0};ev.addr=0x8000;ev.kind='W';ev.value=bridge.predicted.mapper_values[i];
  cyc_trace_cycle_hook(&ev);
 }
 memcpy(ram,bridge.predicted.ram_after,2048);
 cpu.pc=0x1235;cpu.s=0xfe;
 cpu.a=bridge.predicted.a_after;cpu.x=bridge.predicted.x_after;
 cpu.y=bridge.predicted.source_y_after;cpu.c=1;cpu.z=1;cpu.n=0;cpu.v=1;
 tick=1112;
 cyc_cpu_rts_observer();
 assert(bridge.passed==1 && bridge.mismatched==0 && !bridge.active);
 cviii_bridge0660_diagnostics(&stats);
 assert(stats.rts_while_active==2 && stats.unmatched_rts==1);
 printf("PASS hooked entry at original RAM operand, bank-qualified, RTS compare, CPU/mapper unchanged\n");
 return 0;
}