/* JP0656 physically bank-qualified original 6502 PRG0C:$8D15/$8D90-$8DCC.
 * Entire original $FB-$FF control opcode family, read-only state delta.
 * No retail byte payload, playback hooks, or claim of live mapper state. */
#include "cviii_phrase_control_0656.h"
int cviii_phrase_control_0656(const uint8_t *bank, size_t bank_len,
    const uint8_t *ram, size_t ram_len, uint8_t x,
    uint8_t incoming_overflow, CviiiPhraseControl0656 *out) {
    CviiiPhraseControl0656 r={0};
    unsigned cursor, source, pointer, new_cursor;
    uint8_t token, count, next_y, param;
    if (!bank || !ram || !out || bank_len != 16384u ||
        ram_len != 2048u || incoming_overflow > 1u) return -1;
    cursor=ram[0x06AAu + x];
    pointer=(unsigned)ram[0xE2u] | ((unsigned)ram[0xE3u] << 8);
    source=(pointer + cursor)&65535u;
    if (source < 0x8000u || source >= 0xC000u) return -1;
    token=bank[source - 0x8000u];
    if (token < 0xFBu) return 0;
    r.token=token;
    r.token_cpu=(uint16_t)source;
    r.x=x;
    r.y=(uint8_t)cursor;
    r.overflow=incoming_overflow;
    r.cursor=(uint8_t)cursor;
    r.repeat_start=ram[0x06A0u+x];
    r.repeat_counter=ram[0x06AFu+x];
    r.high_nibble=ram[0x03F2u+x];
    r.next_pc=0x8D06u;
    if (token < 0xFEu) {
        /* Source $8D94-$8D9F: INC $06AA,X / LDA $06AA,X /
         * STA $06A0,X / JMP $8D06. CMP #$FE left C=0. */
        new_cursor=(cursor+1u)&255u;
        r.kind=1;
        r.cursor=(uint8_t)new_cursor;
        r.repeat_start=(uint8_t)new_cursor;
        r.write_cursor=r.write_repeat_start=1;
        r.a=(uint8_t)new_cursor;
        r.carry=0;
        r.zero=(uint8_t)(r.a == 0u);
        r.negative=(uint8_t)((r.a & 0x80u)!=0u);
    } else if (token == 0xFEu) {
        /* Source $8DA2-$8DC6. Original INY always wraps before reading
         * FE parameter; caller must prove that address is in raw0C. */
        next_y=(uint8_t)(cursor+1u);
        source=(pointer+next_y)&65535u;
        if (source < 0x8000u || source >= 0xC000u) return -1;
        param=bank[source - 0x8000u];
        count=(uint8_t)(ram[0x06AFu+x]+1u);
        r.y=next_y;
        r.parameter=param;
        r.carry=(uint8_t)(param>=count);
        r.zero=(uint8_t)(param==count);
        r.negative=(uint8_t)((((unsigned)param-(unsigned)count)&128u)!=0u);
        r.write_cursor=1;
        if (param == count) {
            /* Source $8DAD-$8DBD: clear two repetition fields,
             * then increment source cursor twice. The final N/Z
             * are set by the second INC, not by LDA #$00. */
            r.kind=2;
            r.a=0;
            r.repeat_counter=0;
            r.repeat_start=0;
            r.write_repeat_counter=r.write_repeat_start=1;
            r.cursor=(uint8_t)(cursor+2u);
            r.zero=(uint8_t)(r.cursor == 0u);
            r.negative=(uint8_t)((r.cursor & 0x80u)!=0u);
        } else {
            /* $8DBE-$8DC6 LDA $06A0,X; STA $06AA,X. */
            r.kind=3;
            r.a=ram[0x06A0u+x];
            r.cursor=r.a;
            r.repeat_counter=count;
            r.write_repeat_counter=1;
            r.zero=(uint8_t)(r.a == 0u);
            r.negative=(uint8_t)((r.a & 0x80u)!=0u);
        }
    } else {
        /* $8DC7-$8DCC LDA #$FF; STA $03F2,X; RTS. */
        r.kind=4;
        r.a=0xFFu;
        r.carry=1;
        r.zero=0;
        r.negative=1;
        r.high_nibble=0xFFu;
        r.write_high_nibble=1;
        r.next_pc=0x8DCCu; /* RTS executed: parent return address unknown. */
    }
    *out=r;
    return 1;
}
