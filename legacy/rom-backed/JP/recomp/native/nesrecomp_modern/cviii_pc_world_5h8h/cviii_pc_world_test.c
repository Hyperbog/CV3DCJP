#include "cviii_pc_world.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t tile(const CviiiPcRoom *r,int l,int x,int y) {
    uint16_t out=UINT16_MAX;
    assert(cviii_pc_get_tile(r,(CviiiPcLayer)l,(uint16_t)x,(uint16_t)y,&out));
    return out;
}
int main(void) {
    const char *file="cviii_pc_world_test.state";
    CviiiPcWorld *w=cviii_pc_new(UINT64_C(0x1234567890abcdef));
    assert(w && cviii_pc_add_room(w,0,1,1,100,90));
    assert(cviii_pc_add_room(w,0,1,2,32,16));
    assert(!cviii_pc_add_room(w,0,1,2,32,16));
    CviiiPcRoom *tower=cviii_pc_room(w,0,1,1);
    assert(cviii_pc_room(w,0,1,2)!=NULL);
    assert(!cviii_pc_world_to_cell(tower,1,1,(uint16_t[1]){0},(uint16_t[1]){0}));
    assert(cviii_pc_place_room(w,0,1,1,-800,1200));
    assert(cviii_pc_place_room(w,0,1,2,0,800));
    assert(cviii_pc_set_base(tower,CVIII_PC_TERRAIN,7,9,18));
    assert(cviii_pc_set_base(tower,CVIII_PC_DECOR,7,9,93));
    assert(cviii_pc_set_base(tower,CVIII_PC_FRONT,7,9,107));
    assert(tile(tower,0,7,9)==18 && tile(tower,1,7,9)==93 && tile(tower,2,7,9)==107);
    uint16_t cx=0,cy=0; int64_t wx=0,wy=0;
    assert(cviii_pc_world_to_cell(tower,-800+7*8+5,1200+9*8+7,&cx,&cy));
    assert(cx==7 && cy==9);
    assert(cviii_pc_cell_to_world(tower,7,9,&wx,&wy));
    assert(wx==-744 && wy==1272);
    assert(cviii_pc_room_at(w,0,-744,1272)==tower);
    assert(cviii_pc_room_at(w,0,900,1272)==NULL);
    assert(!cviii_pc_world_to_cell(tower,-801,1272,&cx,&cy));
    assert(!cviii_pc_world_to_cell(tower,-800+800,1272,&cx,&cy));
    assert(!cviii_pc_place_room(w,0,1,1,INT64_MAX-1,3));
    assert(cviii_pc_change_tile(tower,CVIII_PC_TERRAIN,7,9,0,CVIII_PC_STAGE));
    assert(tile(tower,0,7,9)==0 && tile(tower,1,7,9)==93);
    assert(cviii_pc_change_tile(tower,CVIII_PC_FRONT,8,9,2,CVIII_PC_VISIT));
    assert(cviii_pc_add_entity(w,0,1,1,19,2,88,94,1));
    uint64_t id=cviii_pc_entity_id(0,1,1,19);
    assert(id==cviii_pc_entity(w,id)->id);
    assert(cviii_pc_entity_world_position(w,id,&wx,&wy));
    assert(wx==-712 && wy==1294);
    assert(!cviii_pc_add_entity(w,0,1,1,19,2,88,94,1));
    assert(cviii_pc_entity_state(w,id,3,CVIII_PC_SESSION));
    cviii_pc_enter_room(w,0,1,1);
    cviii_pc_enter_room(w,0,1,2);
    assert(tile(tower,0,7,9)==0); /* stage mutation persists */
    assert(tile(tower,2,8,9)==0); /* visit-only expires */
    assert(cviii_pc_entity(w,id)->state==3); /* session mutation persists */
    assert(cviii_pc_save(w,file));
    assert(cviii_pc_change_tile(tower,CVIII_PC_TERRAIN,7,9,99,CVIII_PC_STAGE));
    assert(cviii_pc_entity_state(w,id,1,CVIII_PC_STAGE));
    assert(cviii_pc_load(w,file));
    tower=cviii_pc_room(w,0,1,1);
    assert(tile(tower,0,7,9)==0 && cviii_pc_entity(w,id)->state==3);
    /* Wrong asset identity must not mutate the world. */
    CviiiPcWorld *wrong=cviii_pc_new(1234);
    assert(wrong && cviii_pc_add_room(wrong,0,1,1,100,90));
    assert(!cviii_pc_load(wrong,file));
    cviii_pc_free(wrong);
    /* A malformed save must not partially apply its first valid record. */
    FILE *bad=fopen(file,"wb"); assert(bad);
    fputs("CVIII-PCSTATE-1 1234567890abcdef 1 0 1 2\n",bad);
    fputs("T 0 1 1 7 9 0 13 2\nX bad\n",bad); assert(!fclose(bad));
    assert(!cviii_pc_load(w,file));
    assert(tile(tower,0,7,9)==0);
    remove(file);
    cviii_pc_enter_room(w,1,0,0);
    assert(tile(tower,0,7,9)==18); /* stage expiry */
    assert(cviii_pc_entity(w,id)->state==3); /* session still persists */
    cviii_pc_free(w);
    puts("PASS 5H-8H: 2D room coords, 3 layers, stable IDs, lifetimes, save/load rollback");
    return 0;
}
