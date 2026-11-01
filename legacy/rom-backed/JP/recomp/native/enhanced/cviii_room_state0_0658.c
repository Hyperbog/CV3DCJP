#include "cviii_room_state0_0658.h"
#include <string.h>
static const uint16_t clears[][2] = {
  {0x004Du,0x00E0u},{0x00F0u,0x00F8u},
  {0x0300u,0x03C0u},{0x0400u,0x0500u},{0x0500u,0x0600u},
  {0x0600u,0x06A0u},{0x06E0u,0x0700u},{0x0700u,0x07F6u}
};
int cviii_room_state0_0658(const uint8_t *raw0a,size_t raw_len,
 const uint8_t *fixed,size_t fixed_len,const uint8_t *ram,size_t ram_len,
 CviiiRoomState0_0658 *out) {
    CviiiRoomState0_0658 r;
    uint8_t saved;
    unsigned i, address, cnt=0;
    if(!raw0a || !fixed || !ram || !out || raw_len!=16384u ||
       fixed_len!=8192u || ram_len!=2048u || ram[0x18]!=0x0Du ||
       ram[0x19]!=0u || ram[0x23]!=0x0Au) return -1;
    /* The caller must source-authenticate BOTH ROM physical banks.
     * Parameter bank0A is present to enforce explicit mapper context;
     * it is not used as a cross-bank pointer by the native helper. */
    (void)raw0a;
    memset(&r,0,sizeof r);
    if(cviii_room_ppu_stream_0658(fixed,fixed_len,ram[0xFF],
          ram[0xFE],&r.ppu)!=1) return -1;
    memcpy(r.ram_after,ram,2048u);
    saved=ram[0x781u];
    r.ram_after[0x21u]=0x20u;
    r.ram_after[0x6Au]=0;
    r.ram_after[0x43u]=0;
    r.ram_after[0x40u]=0;
    r.ram_after[0x6Fu]=0;
    for(i=0;i<sizeof clears/sizeof clears[0];i++) {
       for(address=clears[i][0];address<clears[i][1];address++) {
          r.ram_after[address]=0;
          cnt++;
       }
    }
    r.ram_after[0x781u]=saved;
    /* $EB82 sets $00/$01 to the last stream byte ($EC46) and the
     * stream iterator's $02 to the final repeat count ($40 in JP).
     * The original PPU stream is SHA-authenticated privately. */
    r.ram_after[0x00u]=(uint8_t)r.ppu.final_source_pointer;
    r.ram_after[0x01u]=(uint8_t)(r.ppu.final_source_pointer>>8);
    r.ram_after[0x02u]=r.ppu.scratch02_final_count;
    r.ram_after[0xFCu]=0;
    r.ram_after[0xFDu]=0;
    r.ram_after[0x19u]=(uint8_t)(ram[0x19u]+1u);
    r.cpu_ram_bank_after=ram[0x23u];
    r.state_after=r.ram_after[0x19u];
    r.mapper_reg_address=0xC000u;
    r.mapper_reg_value=0x1Eu;
    r.sound_reg_address=0xB003u;
    r.sound_reg_value=0x20u;
    r.distinct_cleared_bytes=(uint16_t)cnt;
    *out=r;
    return 1;
}
