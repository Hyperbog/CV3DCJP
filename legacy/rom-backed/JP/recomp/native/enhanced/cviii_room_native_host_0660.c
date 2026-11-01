/* Optional NESRecomp guest-observation adapter: no instruction substitution. */
#include "cviii_room_native_host_0660.h"
#include "cpu6502.h"
#include "cyc_core.h"
#include "cyc_trace.h"
#include "hw.h"
#include <stdio.h>
#include <stdlib.h>

static CviiiRoomNativeBridge0660 *observed;
static int trace_owned;
static int summary_registered;
static CviiiRoomNativeHostDiagnostics0660 diagnostics;

void cviii_bridge0660_diagnostics(CviiiRoomNativeHostDiagnostics0660 *out) {
    if (out) *out=diagnostics;
}
/* Normal CRT shutdown only: force-killing the EXE skips atexit(). */
static void summarize(void) {
    const CviiiRoomNativeBridge0660 *b=observed;
    if (!b) return;
    fprintf(stderr,
        "[CVIII native bridge summary] operand_reads=%llu site_reads=%llu "
        "wrong_bank=%llu pending_interrupt=%llu wrong_context=%llu "
        "wrong_opcode=%llu unsupported=%llu entries=%llu "
        "rts_checked=%llu unmatched_rts=%llu matched=%llu "
        "mismatched=%llu incomplete_or_untraced=%llu active=%u\n",
        (unsigned long long)diagnostics.operand_reads,
        (unsigned long long)diagnostics.site_reads,
        (unsigned long long)diagnostics.wrong_bank,
        (unsigned long long)diagnostics.pending_interrupt,
        (unsigned long long)diagnostics.wrong_context,
        (unsigned long long)diagnostics.wrong_opcode,
        (unsigned long long)b->skipped,
        (unsigned long long)b->observed_entries,
        (unsigned long long)diagnostics.rts_while_active,
        (unsigned long long)diagnostics.unmatched_rts,
        (unsigned long long)b->passed,
        (unsigned long long)b->mismatched,
        (unsigned long long)b->partial,
        (unsigned)b->active);
}

static uint8_t operand(uint16_t pc,uint16_t addr,uint8_t value) {
    CviiiRoomNativeBridge0660 *b=observed;
    if (!b || !b->ready) return value;
    diagnostics.operand_reads++;
    if (pc!=CVIII_0660_ENTRY_PC || addr!=0x7A0u) return value;
    diagnostics.site_reads++;
    if (hw_prg_bank4(pc)!=CVIII_0660_BANK4) {
        diagnostics.wrong_bank++;
        return value;
    }
    if (cpu.do_nmi | cpu.do_irq | cpu.do_reset) {
        diagnostics.pending_interrupt++;
        return value;
    }
    const uint8_t *ram=cyc_cpu_ram();
    if (!ram || ram[0x23u]!=0x0Au) {
        diagnostics.wrong_context++;
        return value;
    }
    uint8_t opcode=0;
    if (!cyc_debug_peek(pc,&opcode) || opcode!=0xADu ||
        !cyc_debug_peek((uint16_t)(pc+1u),&opcode) || opcode!=0xA0u ||
        !cyc_debug_peek((uint16_t)(pc+2u),&opcode) || opcode!=0x07u) {
        diagnostics.wrong_opcode++;
        return value;
    }
    (void)cviii_bridge0660_begin(b,ram,2048u,cpu.s,cpu.v,cpu.i,cpu.d,
        cyc_cycle_count(), trace_owned && cyc_trace_enabled);
    return value; /* always original value; no guest mutation */
}
static void mapper_cycle(const CycTraceCycle *cycle) {
    if(observed && cycle && !cycle->dma && !cycle->halt &&
       cycle->kind=='W' && cycle->addr==0x8000u)
       cviii_bridge0660_mapper_write(observed,cycle->value);
}
static void rts_completed(void) {
    CviiiRoomNativeBridge0660 *b=observed;
    if(!b || !b->active) return;
    diagnostics.rts_while_active++;
    const uint8_t *ram=cyc_cpu_ram();
    int result=cviii_bridge0660_rts(b,ram,2048u,cpu.pc,cpu.a,cpu.x,
        cpu.y,cpu.s,cpu.c,cpu.z,cpu.n,cpu.v,cpu.i,cpu.d,
        hw_prg_bank4(CVIII_0660_ENTRY_PC));
    if(!result) diagnostics.unmatched_rts++;
    if(result) {
        unsigned long long elapsed=(unsigned long long)
            (cyc_cycle_count()-b->entered_cycles);
        fprintf(stderr,"[CVIII native bridge $0A:$B7F7] selector=%u cycles_after_LDA=%llu "
                       "RAM/CPU=%s mapper=%s; original execution unchanged\n",
            (unsigned)b->selector,elapsed,
            result<0?"MISMATCH":"MATCH",
            result==1?"ORDER_MATCH":result==2?"NOT_OBSERVED":"MISMATCH");
    }
}
int cviii_bridge0660_attach(CviiiRoomNativeBridge0660 *b) {
    if(!b || !b->ready || (observed && observed!=b)) return 0;
    /* Do not displace existing tooling; any collision disables verification. */
    if((cyc_cpu_ram_read_policy && cyc_cpu_ram_read_policy!=operand) ||
       (cyc_cpu_rts_observer && cyc_cpu_rts_observer!=rts_completed)) return 0;
    if (!summary_registered) {
        if (atexit(summarize)!=0) return 0;
        summary_registered=1;
    }
    observed=b;
    cyc_cpu_ram_read_policy=operand;
    cyc_cpu_rts_observer=rts_completed;
    if(!cyc_trace_cycle_hook || cyc_trace_cycle_hook==mapper_cycle) {
        cyc_trace_cycle_hook=mapper_cycle;
        trace_owned=1;
    } else trace_owned=0; /* RAM/CPU parity still possible; no mapper claim */
    fprintf(stderr,"[CVIII native bridge $0A:$B7F7] attached; guest execution unchanged\n");
    return 1;
}
void cviii_bridge0660_boundary(CviiiRoomNativeBridge0660 *b) {
    cviii_bridge0660_reset(b);
}