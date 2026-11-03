#include "CollisionLive.h"
#include <string.h>
#include <limits.h>

/* Physical bank 14 (CPU $8000-$BFFF) starts at file $38010.
 * Fixed physical bank 15 (CPU $C000-$FFFF) starts at file $3C010.
 */
#define JP_BANK14_FILE 0x38010u
#define JP_FIXED_FILE  0x3C010u
#define JP_QUERY_PC    0xFCDEu

CollisionLiveStatus CollisionLive_ProbeFCDE(
    const CollisionLiveSnapshot *before,
    uint16_t caller_pc,
    const uint8_t *rom,size_t rom_bytes,
    CollisionLivePreview *preview)
{
    if(!preview) return COLLISION_LIVE_INVALID_ARGUMENT;
    memset(preview,0,sizeof *preview);
    preview->status=COLLISION_LIVE_INVALID_ARGUMENT;
    if(!before||!rom||rom_bytes!=COLLISION_ROM_BYTES)
        return preview->status;
    if(before->pc!=JP_QUERY_PC){
        preview->status=COLLISION_LIVE_WRONG_ENTRY;
        return preview->status;
    }
    if(before->cpu.mapped_prg_bank!=14u||before->cpu.ram[0x23u]!=14u){
        preview->status=COLLISION_LIVE_WRONG_BANK;
        return preview->status;
    }
    if(before->interrupt_pending||before->dma_active||before->mapper_irq_pending){
        preview->status=COLLISION_LIVE_ASYNC_HARDWARE;
        return preview->status;
    }
    if(caller_pc<0x8000u||caller_pc>0xBFFDu){
        preview->status=COLLISION_LIVE_UNVERIFIED_CALLER;
        return preview->status;
    }
    size_t offset=JP_BANK14_FILE+(size_t)(caller_pc-0x8000u);
    if(rom[offset]!=0x20u||rom[offset+1]!=0xDEu||rom[offset+2]!=0xFCu){
        preview->status=COLLISION_LIVE_UNVERIFIED_CALLER;
        return preview->status;
    }
    uint8_t sp=before->cpu.sp;
    uint16_t stack_return=(uint16_t)before->cpu.ram[0x100u+(uint8_t)(sp+1u)]|
        (uint16_t)((uint16_t)before->cpu.ram[0x100u+(uint8_t)(sp+2u)]<<8);
    if(stack_return!=(uint16_t)(caller_pc+2u)){
        preview->status=COLLISION_LIVE_STACK_MISMATCH;
        return preview->status;
    }
    /* Work only on the copied CPU state; rejected native paths never affect
     * the original. No CPU clock is modified in the host. */
    CollisionLiveSnapshot trial=*before;
    CollisionTimedExit r=Collision_FCDE_Timed(&trial.cpu,rom+JP_FIXED_FILE);
    if(!r.exit.ok||r.exit.kind!=COLLISION_RTS||!r.cpu_cycles||
       (r.exit.next_pc!=0xFC7Fu&&r.exit.next_pc!=0xFD3Fu&&
        r.exit.next_pc!=0xFD49u&&r.exit.next_pc!=0xFD4Cu)||
       UINT64_MAX-before->cpu_cycles<r.cpu_cycles){
        preview->status=COLLISION_LIVE_NATIVE_REJECTED;
        return preview->status;
    }
    /* Collision_FCDE_Timed includes the CPU's six-cycle RTS in its count,
     * but ends BEFORE the RTS pop. Complete that one opcode's state effect. */
    trial.cpu.sp=(uint8_t)(trial.cpu.sp+2u);
    trial.pc=(uint16_t)(stack_return+1u);
    trial.cpu_cycles+=r.cpu_cycles;
    preview->after=trial;
    preview->caller_pc=caller_pc;
    preview->query=r;
    preview->status=COLLISION_LIVE_READY_FOR_SHADOW;
    preview->runtime_commit_authorized=0; /* Mandatory: no bus-cycle proof. */
    return preview->status;
}
