/* No hardware interaction; original 6502 remains the sole executor. */
#include "cviii_room_native_bridge_0660.h"
#include <stdio.h>
#include <string.h>

static const uint8_t source_sha[32] = {
    0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,
    0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
    0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,
    0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28
};
static uint16_t word(const uint8_t *p) {
    return (uint16_t)(p[0] | (unsigned)p[1] << 8);
}
static uint64_t hash(const uint8_t *p, size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++) h=(h ^ p[i]) * UINT64_C(1099511628211);
    return h;
}
int cviii_bridge0660_load(CviiiRoomNativeBridge0660 *b, const char *path) {
    if(!b || !path || !*path) return 0;
    FILE *f=fopen(path,"rb");
    if(!f) return 0;
    uint8_t p[CVIII_0660_PACK_SIZE];
    size_t n=fread(p,1,sizeof p,f);
    int extra=fgetc(f),error=ferror(f);
    fclose(f);
    const uint8_t magic[8]={'C','V','N','0','6','6','0',0};
    const uint64_t pinned=UINT64_C(0xCC5C6802DEB10072);
    uint64_t declared=0;
    if(n==sizeof p) for(unsigned i=0;i<8;i++) declared|=(uint64_t)p[12+i] << (8u*i);
    if(n!=sizeof p || extra!=EOF || error || memcmp(p,magic,8) ||
       word(p+8)!=1 || word(p+10)!=CVIII_0660_PAYLOAD_SIZE ||
       memcmp(p+20,source_sha,32) || declared!=pinned ||
       hash(p+64,CVIII_0660_PAYLOAD_SIZE)!=pinned) return 0;
    for(unsigned i=52;i<64;i++) if(p[i]) return 0;
    /* The only values exposed are the ROM-authenticated table and stream.
     * No original bytes are committed, and all other bank bytes remain 0. */
    CviiiRoomNativeBridge0660 candidate={0};
    memcpy(candidate.bank0a+(0xB8C2u-0x8000u),p+64,10);
    memcpy(candidate.bank0e+(0xBE8Bu-0x8000u),p+74,175);
    for(unsigned sel=0;sel<5;sel++) {
        uint8_t ram[2048]={0};
        CviiiRoomStream0659 output;
        ram[0x23]=0x0a; ram[0x7a0]=(uint8_t)sel;
        if(cviii_room_stream_0659(candidate.bank0a,sizeof candidate.bank0a,
             candidate.bank0e,sizeof candidate.bank0e,ram,sizeof ram,0,&output)!=1)
            return 0;
    }
    candidate.ready=1;
    *b=candidate; /* Atomic on success; preserve a valid previous pack on failure. */
    return 1;
}
void cviii_bridge0660_reset(CviiiRoomNativeBridge0660 *b) {
    if(!b) return;
    if(b->active) b->partial++; /* incomplete call on power/load boundary */
    b->active=0;
    b->actual_writes=0;
    b->event_overflow=0;
}
int cviii_bridge0660_begin(CviiiRoomNativeBridge0660 *b,
    const uint8_t *ram, size_t ram_size, uint8_t s,
    uint8_t overflow, uint8_t irq_disable, uint8_t decimal,
    uint64_t cycles, int trace_available)
{
    if(!b || !b->ready || !ram || ram_size!=2048 || overflow>1 ||
       irq_disable>1 || decimal>1) return 0;
    if(b->active) cviii_bridge0660_reset(b);
    CviiiRoomStream0659 predicted;
    const int ok=cviii_room_stream_0659(b->bank0a,sizeof b->bank0a,
        b->bank0e,sizeof b->bank0e,ram,ram_size,overflow,&predicted);
    if(ok!=1) { b->skipped++; return 0; }
    b->predicted=predicted;
    b->return_pc=(uint16_t)((unsigned)ram[0x100u+(uint8_t)(s+1u)] |
             (unsigned)ram[0x100u+(uint8_t)(s+2u)]<<8u);
    b->return_pc++;
    b->entry_s=s;
    b->entry_i=irq_disable;
    b->entry_d=decimal;
    b->selector=ram[0x7A0u];
    b->entered_cycles=cycles;
    b->actual_writes=0;
    b->event_overflow=0;
    b->trace_at_entry=trace_available!=0;
    b->active=1;
    b->observed_entries++;
    return 1;
}
void cviii_bridge0660_mapper_write(CviiiRoomNativeBridge0660 *b,uint8_t value) {
    if(!b || !b->active || !b->trace_at_entry) return;
    if(b->actual_writes>=CVIII_0659_MAPPER_MAX) {
        b->event_overflow=1;
        return;
    }
    b->actual_mapper[b->actual_writes++]=value;
}
int cviii_bridge0660_rts(CviiiRoomNativeBridge0660 *b,
    const uint8_t *ram,size_t len,uint16_t pc,
    uint8_t a,uint8_t x,uint8_t y,uint8_t s,
    uint8_t c,uint8_t z,uint8_t n,uint8_t v,
    uint8_t i,uint8_t d,unsigned mapped_bank4)
{
    if(!b || !b->active || !ram || len!=2048 ||
       pc!=b->return_pc || s!=(uint8_t)(b->entry_s+2u)) return 0;
    const CviiiRoomStream0659 *q=&b->predicted;
    b->active=0;
    int match=memcmp(ram,q->ram_after,0x100u)==0 &&
              memcmp(ram+0x200u,q->ram_after+0x200u,0x600u)==0 &&
              a==q->a_after && x==q->x_after && y==q->source_y_after &&
              c==q->carry_after && z==q->zero_after &&
              n==q->negative_after && v==q->overflow_after &&
              i==b->entry_i && d==b->entry_d &&
              mapped_bank4==CVIII_0660_BANK4;
    if(b->trace_at_entry) match=match && !b->event_overflow &&
               b->actual_writes==q->mapper_events &&
               !memcmp(b->actual_mapper,q->mapper_values,q->mapper_events);
    if(!match) { b->mismatched++; return -1; }
    if(!b->trace_at_entry) { b->partial++; return 2; }
    b->passed++;
    return 1;
}