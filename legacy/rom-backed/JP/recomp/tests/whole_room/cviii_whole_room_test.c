/* Focused test: import from locally owned ROM; never commit private pack. */
#include "cviii_whole_room.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv)
{
    assert(argc==2);
    CviiiWholeRoomPack pack={0};
    CviiiWholeRoomView v,prior;
    uint8_t tile;
    assert(cviii_whole_room_open(&pack,argv[1]));
    assert(pack.allocation_size>CVIII_WHOLE_ROOM_HEADER);
    assert(cviii_whole_room_find(&pack,0,0,0,&v));
    assert(v.source_rom_offset==0x200BA && v.width==24 && v.height==6);
    assert(v.bg_r5==0x45 && v.bg_r6==0x47);
    assert(cviii_whole_room_tile(&v,0,0,&tile));
    assert(!cviii_whole_room_tile(&v,v.width,0,&tile));
    assert(!cviii_whole_room_tile(&v,0,v.height,&tile));
    assert(!cviii_whole_room_find(&pack,0xff,0xff,0xff,&v));
    assert(cviii_whole_room_find(&pack,10,3,1,&v));
    assert(v.source_rom_offset==0x18a50 && v.width==24 && v.height==6);
    assert(cviii_whole_room_find(&pack,7,0,1,&v));
    assert(v.source_rom_offset==0x1d57b && v.width==32 && v.height==6);
    assert(cviii_whole_room_find(&pack,7,1,0,&v));
    assert(v.source_rom_offset==0x1d6cd && v.width==16 && v.height==6);
    assert(cviii_whole_room_find(&pack,0,1,1,&v));
    assert(v.width==8 && v.height==16 && (v.metadata & 0x80));
    assert(cviii_whole_room_find(&pack,0,2,0,&v));
    assert(v.source_rom_offset==0x202ff); /* selector-corrected room order */
    prior=v;
    uint8_t *bad=(uint8_t *)malloc(pack.allocation_size);
    assert(bad);
    memcpy(bad,pack.allocation,pack.allocation_size);
    bad[pack.allocation_size-1] ^= 1;
    CviiiWholeRoomView scratch[CVIII_WHOLE_ROOM_COUNT];
    assert(!cviii_whole_room_decode(bad,pack.allocation_size,scratch));
    assert(!cviii_whole_room_open(&pack,"missing_private_file.dat"));
    assert(cviii_whole_room_find(&pack,0,2,0,&v));
    assert(v.source_rom_offset==prior.source_rom_offset);
    assert(cviii_whole_room_decode(pack.allocation,pack.allocation_size,scratch));
    bad[pack.allocation_size-1] ^= 1;
    free(bad);
    cviii_whole_room_close(&pack);
    assert(!cviii_whole_room_find(&pack,0,0,0,&v));
    puts("PASS: C11 whole-room pack open/find/tile/rejection/atomicity");
    return 0;
}