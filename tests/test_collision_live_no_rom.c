/* ROM-free failure-path check; no game executable or private ROM required. */
#include "../src/collision/CollisionLive.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    CollisionLiveSnapshot input={0};
    CollisionLivePreview preview={0};
    input.pc=0xFCDE;input.cpu.mapped_prg_bank=14;
    input.cpu.ram[0x23]=14;
    assert(CollisionLive_ProbeFCDE(&input,0x8345,NULL,0,&preview)==COLLISION_LIVE_INVALID_ARGUMENT);
    assert(preview.runtime_commit_authorized==0);
    uint8_t *dummy=(uint8_t*)calloc(COLLISION_ROM_BYTES,1);assert(dummy);
    input.pc=0x8000;
    assert(CollisionLive_ProbeFCDE(&input,0x8345,dummy,COLLISION_ROM_BYTES,&preview)==COLLISION_LIVE_WRONG_ENTRY);
    input.pc=0xFCDE;input.cpu.mapped_prg_bank=7;
    assert(CollisionLive_ProbeFCDE(&input,0x8345,dummy,COLLISION_ROM_BYTES,&preview)==COLLISION_LIVE_WRONG_BANK);
    input.cpu.mapped_prg_bank=14;input.dma_active=1;
    assert(CollisionLive_ProbeFCDE(&input,0x8345,dummy,COLLISION_ROM_BYTES,&preview)==COLLISION_LIVE_ASYNC_HARDWARE);
    input.dma_active=0;
    assert(CollisionLive_ProbeFCDE(&input,0x8345,dummy,COLLISION_ROM_BYTES,&preview)==COLLISION_LIVE_UNVERIFIED_CALLER);
    assert(preview.runtime_commit_authorized==0);
    free(dummy);return 0;
}
