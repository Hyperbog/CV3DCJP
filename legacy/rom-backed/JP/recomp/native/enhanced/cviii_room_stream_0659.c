#include "cviii_room_stream_0659.h"
#include <string.h>
#define CPU_BASE 0x8000u
#define TBL 0xB8C2u
static int bounded_rom(const uint8_t *b, unsigned cpu, unsigned *value) {
    if (cpu < CPU_BASE || cpu > 0xBFFFu) return 0;
    *value = b[cpu - CPU_BASE];
    return 1;
}
/* Note that every logical read passes through original fixed FB96, switching
 * bank 0A -> 0E -> 0A. Preserve the observable sequence as a value plan. */
static int read_source(const uint8_t *source, unsigned root, unsigned y,
                       CviiiRoomStream0659 *out, unsigned *value) {
    unsigned v;
    if (y > 255u || !bounded_rom(source, (root+y)&65535u, &v)) return 0;
    if (out->mapper_events+2u > CVIII_0659_MAPPER_MAX) return 0;
    out->mapper_values[out->mapper_events++] = 0x0Eu;
    out->mapper_values[out->mapper_events++] = 0x0Au;
    out->source_calls++;
    *value = v;
    return 1;
}
int cviii_room_stream_0659(const uint8_t *raw0a, size_t len0a,
    const uint8_t *raw0e, size_t len0e,
    const uint8_t *ram, size_t ram_len, uint8_t incoming_overflow,
    CviiiRoomStream0659 *out) {
    static const uint16_t known_roots[5] = {
        0xBEA4u, 0xBEDBu, 0xBF03u, 0xBE8Bu, 0xBF1Cu
    };
    CviiiRoomStream0659 r;
    unsigned sel, root, low, high, val, y, x, i;
    unsigned nibble0,nibble1;
    if (!raw0a || !raw0e || !ram || !out || len0a!=16384u ||
        len0e!=16384u || ram_len!=2048u || incoming_overflow>1u) return -1;
    if (ram[0x23u]!=0x0Au) return -1;
    sel=ram[0x7A0u];
    if (sel>=5u) return 0; /* $B8C2 two-byte index outside proven 5 roots. */
    if (!bounded_rom(raw0a,TBL+2u*sel,&low) ||
        !bounded_rom(raw0a,TBL+2u*sel+1u,&high)) return -1;
    root=(high<<8u)|low;
    if (root!=known_roots[sel]) return -1;
    memset(&r,0,sizeof r);
    memcpy(r.ram_after,ram,2048u);
    r.source_root=(uint16_t)root;
    r.ram_after[0x08]=low;
    r.ram_after[0x09]=high;
    /* Original source repeats FB96 twice at Y=0, then twice at Y=1. */
    if (!read_source(raw0e,root,0,&r,&nibble0)) return -1;
    r.ram_after[0x34]=(uint8_t)(nibble0>>4u);
    if (!read_source(raw0e,root,0,&r,&val)) return -1;
    r.ram_after[0x35]=(uint8_t)(val&0x0Fu);
    if (!read_source(raw0e,root,1,&r,&nibble1)) return -1;
    r.ram_after[0x36]=(uint8_t)(nibble1>>4u);
    if (!read_source(raw0e,root,1,&r,&val)) return -1;
    r.ram_after[0x54]=(uint8_t)(val&0x0Fu);
    r.ram_after[0x72]=(uint8_t)(val&1u);
    r.ram_after[0x53]=0;
    if (!read_source(raw0e,root,2,&r,&val)) return -1;
    r.ram_after[0x7A1]=(uint8_t)val;
    if (!read_source(raw0e,root,3,&r,&val)) return -1;
    r.ram_after[0x54E]=(uint8_t)val;
    y=4u;x=1u;
    for(i=0;i<=CVIII_0659_MAX_RECORDS;i++) {
        if(!read_source(raw0e,root,y,&r,&val)) return -1;
        if(val==255u) {
            r.record_count=(uint8_t)i;
            r.source_y_after=(uint8_t)y;
            r.x_after=(uint8_t)x;
            r.a_after=255u;
            r.carry_after=1;
            r.zero_after=1;
            r.negative_after=0;
            /* No opcode in this bounded call graph changes 6502 V. */
            r.overflow_after=incoming_overflow;
            r.bank_after=0x0Au;
            *out=r;
            return 1;
        }
        if(i==CVIII_0659_MAX_RECORDS || x>=0x100u) return -1;
        r.ram_after[0x48C+x]=(uint8_t)val;
        y++;
        if(!read_source(raw0e,root,y,&r,&val)) return -1;
        r.ram_after[0x400+x]=(uint8_t)val;
        y++;
        if(!read_source(raw0e,root,y,&r,&val)) return -1;
        r.ram_after[0x438+x]=(uint8_t)val;
        y++;
        if(!read_source(raw0e,root,y,&r,&val)) return -1;
        r.ram_after[0x41C+x]=(uint8_t)val;
        y++;
        if(!read_source(raw0e,root,y,&r,&val)) return -1;
        r.ram_after[0x4A8+x]=(uint8_t)val;
        y++;
        r.ram_after[0x54Eu+x]=1u;
        x++;
    }
    return -1;
}
