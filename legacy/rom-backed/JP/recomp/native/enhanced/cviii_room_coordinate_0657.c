/* Source-authenticated original Japanese Mapper24 raw0A coordinate stage.
 * Original instruction windows $8001-$801D, $80D1-$8107.
 * No embedded original game assets, no stateful write into guest RAM.
 */
#include "cviii_room_coordinate_0657.h"
int cviii_room_coordinate_0657(const uint8_t *bank, size_t bank_len,
        const uint8_t *ram, size_t ram_len, CviiiRoomCoordinate0657 *out) {
    CviiiRoomCoordinate0657 r = {0};
    unsigned low, high, carry, upper, sub, base, mask, signed_part, total, result;
    unsigned x;
    if (!bank || !ram || !out || bank_len != 16384u || ram_len != 2048u)
        return -1;
    /* The host caller is responsible for full ROM hash/provenance; do not
     * silently accept a truncated/different memory buffer. */
    low=(unsigned)ram[0x53u] + 2u;  /* $8001 CLC, ADC #$02 */
    carry = low > 255u;
    high=((unsigned)ram[0x54u] + carry)&255u; /* ADC #$00 */
    upper=((high << 2u) | ((low & 255u) >> 6u)) & 255u;
    x=ram[0x62u];                 /* $80D8 LDX $62 */
    if (x) { sub=5u; base=0u; }
    else   { sub=0u; base=0xC0u; }
    mask=(unsigned)ram[0x53u] & 0x3Fu;
    signed_part=mask & 0x20u ? (mask | 0xC0u) : mask;
    r.room_offset_09=(uint8_t)((signed_part+base)&255u);
    r.room_sub_08=(uint8_t)sub;
    r.masked_0e=(uint8_t)mask;
    total=upper+sub;              /* $80FB LDA $73; CLC; ADC $08 */
    result=total&255u;
    r.room_hi_73=(uint8_t)result;
    r.room_phase_0a=1u;
    r.a=1u; r.x=(uint8_t)x; r.y=(uint8_t)result;
    r.carry=(uint8_t)(total>255u);
    r.overflow=(uint8_t)((~(upper^sub)&(upper^result)&128u)!=0u);
    r.zero=(uint8_t)(result==0u); /* $8106 LDY $73 sets final Z/N */
    r.negative=(uint8_t)((result&128u)!=0u);
    *out=r;
    return 1;
}
