/* NESRecomp cycle-backend read-only FCDE shadow; never handles a call.
 * Authentic bank-14 callers: $8345, $8429, $96E4, $96ED.
 * This is instruction-boundary parity, NOT per-bus-cycle parity.
 */
#include "CollisionDispatchV11.h"
#include "CollisionLive.h"
#include "cyc_core.h"
#include "cpu6502.h"
#include "hw.h"
#include "mod_function_hooks.h"
#include "cyc_trace.h"
#include "hw_internal.h"
#include <stdio.h>
#include <string.h>

#define ENTRY_ID "cv3dcjp.collision.v11.entry"
#define JP_FNV64 UINT64_C(0x76615b120049ad3f)

typedef struct {
    const char *id;
    uint16_t caller, return_pc;
} CollisionHookSiteV11;
/* All caller JSRs and four return opcodes independently verified from the
 * original Japanese ROM. These are NOT a claim of complete caller coverage. */
static const CollisionHookSiteV11 sites[] = {
    {"cv3dcjp.collision.v11.return.8348",0x8345u,0x8348u},
    {"cv3dcjp.collision.v11.return.842c",0x8429u,0x842cu},
    {"cv3dcjp.collision.v11.return.96e7",0x96e4u,0x96e7u},
    {"cv3dcjp.collision.v11.return.96f0",0x96edu,0x96f0u}
};
#define N_SITES (sizeof sites / sizeof sites[0])

static uint8_t rom[COLLISION_ROM_BYTES];
static unsigned loaded, pending;
static uint16_t active_caller, active_return;
static CollisionLivePreview expected;
static CollisionLiveSnapshot started;
static CollisionDispatchV11Stats stats;
static FILE *trace;
static FILE *dma_trace;
static uint64_t dma_cycles, dma_halts, dma_reads, dma_writes;
static uint64_t observed_pairs;
static uint64_t dmc_reads_confirmed, oam_dma_writes;
static ApuDmcView previous_dmc;
static void (*previous_cycle_hook)(const CycTraceCycle *);
static int cycle_hook_installed;

/* Observes exact machine-owned cycle transactions. It never changes them.
 * The existing observer is chained, not discarded. Cycles are collected
 * only while the original $FCDE routine executes (pending = 1).
 * DMA records do not distinguish DMC from OAM without device evidence. */
static void collision_cycle_observer(const CycTraceCycle *cycle)
{
    if (pending && cycle) {
        ApuDmcView dmc;
        apu_debug_dmc(&dmc); /* Device-owned state, observation only. */
        if (cycle->dma) {
            ++dma_cycles;
            if (cycle->halt) ++dma_halts;
            else if (cycle->kind=='R') ++dma_reads;
            else if (cycle->kind=='W') ++dma_writes;
            if (cycle->kind=='W' && cycle->addr==0x2004u)
                ++oam_dma_writes;
            /* Conservative sample-fetch evidence: a DMA read at the old
             * DMC source address and an actual one-byte source advance.
             * One-byte loop resets can evade this lower-bound counter. */
            if (!cycle->halt && cycle->kind=='R' &&
                cycle->addr==previous_dmc.addr &&
                dmc.addr==(uint16_t)(previous_dmc.addr==0xFFFFu
                                      ? 0x8000u : previous_dmc.addr+1u))
                ++dmc_reads_confirmed;
        }
        previous_dmc=dmc;
    }
    if (previous_cycle_hook) previous_cycle_hook(cycle);
}

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
    const CollisionHookSiteV11 *site=NULL;
    for (size_t i=0;i<N_SITES;++i)
        if (stored==(uint16_t)(sites[i].caller+2u)) { site=&sites[i]; break; }
    if (!site) return 0; /* Unknown callers remain on the original CPU. */
    CollisionLiveSnapshot snapshot;
    capture(&snapshot);
    if (CollisionLive_ProbeFCDE(&snapshot,site->caller,rom,sizeof rom,&expected)
        !=COLLISION_LIVE_READY_FOR_SHADOW ||
        expected.runtime_commit_authorized) {
        ++stats.probe_rejected;
        return 0;
    }
    dma_cycles=dma_halts=dma_reads=dma_writes=0;
    dmc_reads_confirmed=oam_dma_writes=0;
    apu_debug_dmc(&previous_dmc);
    started=snapshot;
    active_caller=site->caller;
    active_return=site->return_pc;
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
            (unsigned)active_caller,
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
    if (!pending || pc!=active_return) return 0;
    pending=0;
    if (!loaded || cpu.pc!=pc || !original_mapping() ||
        !cyc_cpu_ram()) { ++stats.mismatched_return; return 0; }
    ++observed_pairs;
    write_trace();
    if (dma_trace) {
        fprintf(dma_trace,"%llu,%u,%llu,%llu,%u,%llu,%llu,%llu,%llu,%llu,%llu\n",
                (unsigned long long)observed_pairs,
                (unsigned)active_caller,
                (unsigned long long)started.cpu_cycles,
                (unsigned long long)cyc_cycle_count(),
                (unsigned)expected.query.cpu_cycles,
                (unsigned long long)dma_cycles,
                (unsigned long long)dma_halts,
                (unsigned long long)dma_reads,
                (unsigned long long)dma_writes,
                (unsigned long long)dmc_reads_confirmed,
                (unsigned long long)oam_dma_writes);
        fflush(dma_trace);
    }
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

int CollisionDispatchV11_DmaTraceOpen(const char *path)
{
    /* --hash-out must activate the pinned engine's actual cycle tracer.
     * Never silently enable/disable tracing or overwrite another callback. */
    if (!cyc_trace_enabled || dma_trace || !path || !*path) return 0;
    if (cyc_trace_cycle_hook==collision_cycle_observer) return 0;
    FILE *existing=fopen(path,"rb");
    if (existing) { fclose(existing); return 0; }
    FILE *f=fopen(path,"wb");
    if (!f) return 0;
    fputs("sample,caller_pc,entry_cycle,return_cycle,instruction_cycles,"
          "dma_cycles,dma_halt_cycles,dma_read_cycles,dma_write_cycles,"
          "dmc_fetches_confirmed,oam_dma_writes\n",f);
    fflush(f);
    dma_trace=f;
    previous_cycle_hook=cyc_trace_cycle_hook;
    cyc_trace_cycle_hook=collision_cycle_observer;
    cycle_hook_installed=1;
    return 1;
}
void CollisionDispatchV11_DmaTraceClose(void)
{
    if (cycle_hook_installed) {
        if (cyc_trace_cycle_hook==collision_cycle_observer)
            cyc_trace_cycle_hook=previous_cycle_hook;
        cycle_hook_installed=0;
        previous_cycle_hook=NULL;
    }
    if (dma_trace) { fclose(dma_trace); dma_trace=NULL; }
}

int CollisionDispatchV11_Register(void)
{
    if (!nes_mod_register_function_entry_plugin(ENTRY_ID,0xFCDEu,fcde_entry))
        return 0;
    for (size_t i=0;i<N_SITES;++i)
        if (!nes_mod_register_function_entry_plugin(sites[i].id,
                sites[i].return_pc,fcde_return))
            return 0;
    return 1;
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
    if (!loaded || !nes_mod_set_function_hook_enabled(ENTRY_ID,1)) return 0;
    size_t i=0;
    for (;i<N_SITES;++i)
        if (!nes_mod_set_function_hook_enabled(sites[i].id,1)) break;
    if (i==N_SITES) return 1;
    while (i>0) (void)nes_mod_set_function_hook_enabled(sites[--i].id,0);
    (void)nes_mod_set_function_hook_enabled(ENTRY_ID,0);
    return 0;
}
void CollisionDispatchV11_GetStats(CollisionDispatchV11Stats *out)
{
    if (out) *out=stats;
}
