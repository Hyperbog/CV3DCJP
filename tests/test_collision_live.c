/* ROM-required regression of the CPU state boundary (not game integration). */
#include "../src/collision/CollisionLive.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
    if(argc!=2){fprintf(stderr,"Usage: test_collision_live Japanese_ROM.nes\n");return 2;}
    FILE *f=fopen(argv[1],"rb");assert(f);
    uint8_t *rom=(uint8_t *)malloc(COLLISION_ROM_BYTES);assert(rom);
    assert(fread(rom,1,COLLISION_ROM_BYTES,f)==COLLISION_ROM_BYTES);
    assert(fgetc(f)==EOF);assert(!fclose(f));
    CollisionLiveSnapshot s={0};
    s.pc=0xFCDE;s.cpu_cycles=1000;
    s.cpu.mapped_prg_bank=14;s.cpu.ram[0x23]=14;
    s.cpu.a=5;s.cpu.x=0x30;s.cpu.sp=0xfb;
    s.cpu.ram[0x1fc]=0x47; /* JSR $8345 pushes $8347 */
    s.cpu.ram[0x1fd]=0x83;
    s.cpu.ram[0x65]=0;s.cpu.ram[0x438]=0x20;
    s.cpu.ram[0x41c]=0x80;s.cpu.ram[0x53]=0x13;
    uint8_t original[sizeof s];memcpy(original,&s,sizeof s);
    CollisionLivePreview out={0};
    assert(CollisionLive_ProbeFCDE(&s,0x8345,rom,COLLISION_ROM_BYTES,&out)==COLLISION_LIVE_READY_FOR_SHADOW);
    assert(out.status==COLLISION_LIVE_READY_FOR_SHADOW);
    assert(out.runtime_commit_authorized==0);
    assert(out.after.pc==0x8348);
    assert(out.after.cpu.sp==0xfd);
    assert(out.after.cpu_cycles==s.cpu_cycles+out.query.cpu_cycles);
    assert(out.query.exit.kind==COLLISION_RTS);
    assert(!memcmp(original,&s,sizeof s)); /* Never apply to live owner. */
    s.cpu.ram[0x1fc]^=1;
    assert(CollisionLive_ProbeFCDE(&s,0x8345,rom,COLLISION_ROM_BYTES,&out)==COLLISION_LIVE_STACK_MISMATCH);
    assert(out.runtime_commit_authorized==0);
    s.cpu.ram[0x1fc]^=1;
    s.dma_active=1;
    assert(CollisionLive_ProbeFCDE(&s,0x8345,rom,COLLISION_ROM_BYTES,&out)==COLLISION_LIVE_ASYNC_HARDWARE);
    s.dma_active=0;s.pc=0xE648;
    assert(CollisionLive_ProbeFCDE(&s,0x8345,rom,COLLISION_ROM_BYTES,&out)==COLLISION_LIVE_WRONG_ENTRY);
    s.pc=0xFCDE;s.cpu.mapped_prg_bank=7;
    assert(CollisionLive_ProbeFCDE(&s,0x8345,rom,COLLISION_ROM_BYTES,&out)==COLLISION_LIVE_WRONG_BANK);
    s.cpu.mapped_prg_bank=14;
    assert(CollisionLive_ProbeFCDE(&s,0x8346,rom,COLLISION_ROM_BYTES,&out)==COLLISION_LIVE_UNVERIFIED_CALLER);
    uint8_t saved=rom[0x3c010+10];rom[0x3c010+10]^=1;
    assert(CollisionLive_ProbeFCDE(&s,0x8345,rom,COLLISION_ROM_BYTES,&out)==COLLISION_LIVE_NATIVE_REJECTED);
    rom[0x3c010+10]=saved;
    free(rom);puts("PASS: guarded FCDE shadow/RTS/clock contract; no live substitution");
    return 0;
}
