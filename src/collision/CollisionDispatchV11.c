/* NESRecomp cycle-backend read-only FCDE shadow; never handles a call.
 * Initial verified slice: bank14 JSR $8345 -> $FCDE -> $8348.
 * This is instruction-boundary parity, NOT per-bus-cycle parity.
 */
#include "CollisionDispatchV11.h"
#include "CollisionLive.h"
#include "cyc_core.h"
#include "cpu6502.h"
#include "hw.h"
#include "mod_function_hooks.h"
#include <stdio.h>
#include <string.h>

#define ENTRY_ID "cv3dcjp.collision.v11.entry"
#define RETURN_ID "cv3dcjp.collision.v11.return.8348"
#define JP_FNV64 UINT64_C(0x76615b120049ad3f)
#define JP_CALLER 0x8345u
#define JP_RETURN 0x8348u

static uint8_t rom[COLLISION_ROM_BYTES];
static unsigned loaded, pending;
static CollisionLivePreview expected;
static CollisionLiveSnapshot started;
static CollisionDispatchV11Stats stats;
static FILE *trace;

static uint64_t rom_fnv(const uint8_t *bytes, size_t count)
{
    uint64_t hash=UINT64_C(0xcbf29ce484222325);
    for (size_t n=0; n<count; ++n) {
        hash ^= bytes[n];
        hash *= UINT64_C(0x100000001b3);
    }
    return hash;
}

/* ROM physical 4-KiB identities for the real Mapper 24 bank-14 callers.
 * Fixed $F000-$FFFF must map to the original final physical bank.
 */
static int original_mapping(void)
{
    return hw_prg_bank4(0x8000)==56u && hw_prg_bank4(0x9000)==57u &&
           hw_prg_bank4(0xA000)==58u && hw_prg_bank4(0xB000)==59u &&
           hw_prg_bank4(0xF000)==63u;
}

static void capture(CollisionLiveSnapshot *snapshot)
{
    const uint8_t *ram=cyc_cpu_ram();
    memset(snapshot,0,sizeof *snapshot);
    snapshot->pc=cpu.pc;
    snapshot->cpu_cycles=cyc_cycle_count();
    snapshot->cpu.a=cpu.a; snapshot->cpu.x=cpu.x;
    snapshot->cpu.y=cpu.y; snapshot->cpu.sp=cpu.s;
    snapshot->cpu.p=cpu_get_p(0);
    memcpy(snapshot->cpu.ram,ram,COLLISION_RAM_BYTES);
    snapshot->cpu.mapped_prg_bank=14u;
    snapshot->interrupt_pending=(uint8_t)!!(cpu.do_irq||cpu.do_nmi||cpu.do_reset);
}

/* Always return 0: never trigger NESRecomp's cycle-skipping RTS shortcut. */
static int fcde_entry(uint16_t pc)
{
    ++stats.entry_hits;
    if (pending) { pending=0; ++stats.abandoned_entry; }
    if (!loaded || pc!=0xFCDEu || cpu.pc!=pc || !original_mapping() ||
        !cyc_cpu_ram()) { ++stats.probe_rejected; return 0; }
    uint8_t sp=cpu.s;
    const uint8_t *ram=cyc_cpu_ram();
    uint16_t stored=(uint16_t)ram[0x100u+(uint8_t)(sp+1u)] |
         (uint16_t)((uint16_t)ram[0x100u+(uint8_t)(sp+2u)]<<8);
    if (stored!=JP_CALLER+2u) return 0;  /* all other known callers are out of slice */
    CollisionLiveSnapshot snapshot;
    capture(&snapshot);
    if (CollisionLive_ProbeFCDE(&snapshot,JP_CALLER,rom,sizeof rom,&expected)
        !=COLLISION_LIVE_READY_FOR_SHADOW ||
        expected.runtime_commit_authorized) {
        ++stats.probe_rejected;
        return 0;
    }
    started=snapshot;
    pending=1;
    ++stats.shadow_ready;
    return 0;
}

static void write_ram(const uint8_t *ram)
{
    static const char hex[]="0123456789abcdef";
    for (unsigned i=0;i<COLLISION_RAM_BYTES;++i) {
        fputc(hex[ram[i]>>4],trace);
        fputc(hex[ram[i]&15u],trace);
    }
}

/* Exactly the 15 original Mesen CSV columns, from the original NESRecomp run.
 * These are guest instruction-boundary observations, not invented cycles. */
static void write_trace(void)
{
    if (!trace) return;
    const CollisionLiveSnapshot *b=&started;
    const uint8_t *ram=cyc_cpu_ram();
    if (!ram) return;
    fprintf(trace,"%u,%llu,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,",
            (unsigned)JP_CALLER,
            (unsigned long long)b->cpu_cycles,
            (unsigned long long)cyc_cycle_count(),
            b->cpu.a,b->cpu.x,b->cpu.y,b->cpu.sp,b->cpu.p,
            cpu.a,cpu.x,cpu.y,cpu.s,cpu_get_p(0));
    write_ram(b->cpu.ram);
    fputc(',',trace);
    write_ram(ram);
    fputc('\n',trace);
    fflush(trace);
}

static int fcde_return(uint16_t pc)
{
    if (!pending) return 0;
    pending=0;
    if (!loaded || pc!=JP_RETURN || cpu.pc!=pc || !original_mapping() ||
        !cyc_cpu_ram()) { ++stats.mismatched_return; return 0; }
    write_trace();
    const CollisionLiveSnapshot *s=&expected.after;
    if (s->pc==cpu.pc && s->cpu_cycles==cyc_cycle_count() &&
        s->cpu.a==cpu.a && s->cpu.x==cpu.x && s->cpu.y==cpu.y &&
        s->cpu.sp==cpu.s && (s->cpu.p&0xCFu)==(cpu_get_p(0)&0xCFu) &&
        memcmp(s->cpu.ram,cyc_cpu_ram(),COLLISION_RAM_BYTES)==0)
        ++stats.matched_return;
    else ++stats.mismatched_return;
    return 0;
}

int CollisionDispatchV11_TraceOpen(const char *path)
{
    if (trace || !path || !*path) return 0;
    FILE *existing=fopen(path,"rb");
    if (existing) { fclose(existing); return 0; }
    trace=fopen(path,"wb");
    if (!trace) return 0;
    fputs("caller_pc,entry_cycle,return_cycle,entry_a,entry_x,entry_y,"
          "entry_sp,entry_p,return_a,return_x,return_y,return_sp,"
          "return_p,entry_ram_hex,return_ram_hex\n",trace);
    fflush(trace);
    return 1;
}
void CollisionDispatchV11_TraceClose(void)
{
    if (trace) { fclose(trace); trace=NULL; }
}

int CollisionDispatchV11_Register(void)
{
    if (!nes_mod_register_function_entry_plugin(ENTRY_ID,0xFCDEu,fcde_entry))
        return 0;
    return nes_mod_register_function_entry_plugin(RETURN_ID,JP_RETURN,fcde_return);
}

int CollisionDispatchV11_LoadRom(const char *path)
{
    loaded=0; pending=0;
    if (!path || !*path) return 0;
    FILE *f=fopen(path,"rb");
    if (!f) return 0;
    size_t bytes=fread(rom,1,sizeof rom,f);
    int trailing=fgetc(f);
    int closed=fclose(f);
    if (bytes!=sizeof rom || trailing!=EOF || closed ||
        rom_fnv(rom,sizeof rom)!=JP_FNV64)
        return 0;
    loaded=1;
    return 1;
}

int CollisionDispatchV11_Enable(void)
{
    if (!loaded) return 0;
    if (!nes_mod_set_function_hook_enabled(ENTRY_ID,1)) return 0;
    if (nes_mod_set_function_hook_enabled(RETURN_ID,1)) return 1;
    (void)nes_mod_set_function_hook_enabled(ENTRY_ID,0);
    return 0;
}
void CollisionDispatchV11_GetStats(CollisionDispatchV11Stats *out)
{
    if (out) *out=stats;
}
