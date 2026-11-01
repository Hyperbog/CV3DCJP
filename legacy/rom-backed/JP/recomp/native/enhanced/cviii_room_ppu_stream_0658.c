#include "cviii_room_ppu_stream_0658.h"
#include <string.h>
#define BASE 0xE000u
#define STREAM_END 0xEC47u
#define SOURCE_PTR 0xEBF7u
static uint8_t byte(const uint8_t *fixed, unsigned address) {
    return fixed[address-BASE];
}
static unsigned word(const uint8_t *fixed,unsigned address) {
    return (unsigned)byte(fixed,address) | ((unsigned)byte(fixed,address+1u)<<8u);
}
int cviii_room_ppu_stream_0658(const uint8_t *fixed, size_t size,
 uint8_t ff, uint8_t fe, CviiiRoomPpuStream0658 *out) {
    CviiiRoomPpuStream0658 r;
    unsigned ptr, ppu, step, command, n, val, i, switch_count = 0;
    if(!fixed || !out || size!=8192u) return -1;
    ptr=word(fixed,SOURCE_PTR);
    if(ptr!=0xEC1Du) return -1;
    memset(&r,0,sizeof r);
    /* Original $E16C: LDA $FF / AND #$7F / STA $2000,
     * $FE AND #$E7 to $2001. The $2002 read at $EB97 clears the
     * latch before two initial $2006 writes from the stream. */
    r.ctrl_setup=(uint8_t)(ff&0x7fu);
    r.ctrl_final=ff;
    r.mask_setup=(uint8_t)(fe&0xe7u);
    r.original_ppu_status_reads=1u;
    if(ptr+2u>STREAM_END) return -1;
    r.initial_ppu_address_low=byte(fixed,ptr);
    r.initial_ppu_address_high=byte(fixed,ptr+1u);
    /* Original $EB9B uses Y=1 first, so send high then low. */
    ppu=((unsigned)r.initial_ppu_address_high << 8u) |
         (unsigned)r.initial_ppu_address_low;
    ppu &= 0x3fffu;
    ptr+=2u; switch_count=1u;
    step=(ff&4u)?32u:1u;
    while(ptr<STREAM_END) {
        command=byte(fixed,ptr);
        if(command==0xffu) {
            if(ptr!=STREAM_END-1u || r.count!=CVIII_0658_MAX_WRITES ||
               switch_count!=2u) return -1;
            r.final_source_pointer=(uint16_t)ptr;
            r.final_vram_address=(uint16_t)ppu;
            r.address_switch_count=(uint8_t)switch_count;
            r.consumed_stream_bytes=(uint8_t)(ptr-0xEC1Du+1u);
            *out=r;
            return 1;
        }
        if(command==0x7fu) {
            /* Original $EBEC advances past $7F and re-enters EB97;
             * PPUADDR source pair follows as two raw bytes. */
            if(ptr+3u>STREAM_END || switch_count!=1u) return -1;
            ptr++;
            r.second_ppu_address_low=byte(fixed,ptr);
            r.second_ppu_address_high=byte(fixed,ptr+1u);
            ppu=(((unsigned)r.second_ppu_address_high << 8u) |
                  (unsigned)r.second_ppu_address_low) & 0x3fffu;
            ptr+=2u;
            r.original_ppu_status_reads++;
            switch_count++;
            continue;
        }
        if(command==0u || ptr+2u>STREAM_END) return -1;
        n=command&0x7fu;
        if(n==0u || n>CVIII_0658_MAX_WRITES-(unsigned)r.count) return -1;
        /* On negative control bytes $81-$FE, the original $EBBF
         * writes one literal byte per source item. The authentic
         * retail clear stream uses the positive run form only. */
        if(command&0x80u) {
            if(ptr+1u+n>STREAM_END) return -1;
            for(i=0;i<n;i++) {
                r.address[r.count]=(uint16_t)ppu;
                r.value[r.count]=byte(fixed,ptr+1u+i);
                r.count++;
                ppu=(ppu+step)&0x3fffu;
            }
            ptr+=1u+n;
        } else {
            val=byte(fixed,ptr+1u);
            for(i=0;i<n;i++) {
                r.address[r.count]=(uint16_t)ppu;
                r.value[r.count]=(uint8_t)val;
                r.count++;
                ppu=(ppu+step)&0x3fffu;
            }
            ptr+=2u;
        }
        r.scratch02_final_count=(uint8_t)n;
    }
    return -1;
}
