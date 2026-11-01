#include "cviii_pc_world.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool valid_scope(CviiiPcScope s) { return s>=CVIII_PC_VISIT && s<=CVIII_PC_SESSION; }
static bool in_room(const CviiiPcRoom *r, uint16_t x, uint16_t y) {
    return r && r->cells && x<r->width && y<r->height;
}
static CviiiPcTile *cell(CviiiPcRoom *r, uint16_t x, uint16_t y) {
    return &r->cells[(size_t)y*r->width+x];
}
CviiiPcWorld *cviii_pc_new(uint64_t signature) {
    CviiiPcWorld *w=(CviiiPcWorld*)calloc(1,sizeof(*w));
    if(w) w->resource_signature=signature;
    return w;
}
void cviii_pc_free(CviiiPcWorld *w) {
    if(!w) return;
    for(size_t i=0;i<w->room_count;i++) free(w->rooms[i].cells);
    free(w);
}
CviiiPcRoom *cviii_pc_room(CviiiPcWorld *w,uint8_t stage,uint8_t area,uint8_t room) {
    if(!w) return NULL;
    for(size_t i=0;i<w->room_count;i++) {
        CviiiPcRoom *r=&w->rooms[i];
        if(r->stage==stage && r->area==area && r->room==room) return r;
    }
    return NULL;
}
const CviiiPcRoom *cviii_pc_room_const(const CviiiPcWorld *w,uint8_t s,uint8_t a,uint8_t r) {
    return cviii_pc_room((CviiiPcWorld*)w,s,a,r);
}
bool cviii_pc_add_room(CviiiPcWorld *w,uint8_t s,uint8_t a,uint8_t id,
                       uint16_t width,uint16_t height) {
    if(!w || !width || !height || (size_t)width*height>65536 ||
       w->room_count==CVIII_PC_MAX_ROOMS || cviii_pc_room(w,s,a,id)) return false;
    CviiiPcTile *cells=(CviiiPcTile*)calloc((size_t)width*height,sizeof(*cells));
    if(!cells) return false;
    for(size_t i=0;i<(size_t)width*height;i++)
        for(unsigned layer=0;layer<CVIII_PC_LAYER_COUNT;layer++)
            cells[i].changed[layer]=CVIII_PC_TILE_UNSET;
    CviiiPcRoom *r=&w->rooms[w->room_count++];
    *r=(CviiiPcRoom){.stage=s,.area=a,.room=id,.width=width,.height=height,.cells=cells};
    return true;
}
bool cviii_pc_place_room(CviiiPcWorld *w,uint8_t s,uint8_t a,uint8_t id,
                         int64_t x,int64_t y) {
    CviiiPcRoom *r=cviii_pc_room(w,s,a,id);
    if(!r || x>INT64_MAX-(int64_t)r->width*8 ||
       y>INT64_MAX-(int64_t)r->height*8) return false;
    r->world_x=x; r->world_y=y; r->placed=true;
    return true;
}
bool cviii_pc_world_to_cell(const CviiiPcRoom *r,int64_t wx,int64_t wy,
                            uint16_t *cx,uint16_t *cy) {
    if(!r || !r->placed || !cx || !cy || wx<r->world_x || wy<r->world_y) return false;
    /* No signed subtraction can overflow: upper bounds checked in place_room. */
    if(wx>=r->world_x+(int64_t)r->width*8 ||
       wy>=r->world_y+(int64_t)r->height*8) return false;
    *cx=(uint16_t)((wx-r->world_x)/8);
    *cy=(uint16_t)((wy-r->world_y)/8);
    return true;
}
bool cviii_pc_cell_to_world(const CviiiPcRoom *r,uint16_t x,uint16_t y,
                            int64_t *wx,int64_t *wy) {
    if(!r || !r->placed || x>=r->width || y>=r->height || !wx || !wy) return false;
    *wx=r->world_x+(int64_t)x*8; *wy=r->world_y+(int64_t)y*8;
    return true;
}
const CviiiPcRoom *cviii_pc_room_at(const CviiiPcWorld *w,uint8_t stage,
                                     int64_t wx,int64_t wy) {
    if(!w) return NULL;
    const CviiiPcRoom *found=NULL;
    for(size_t i=0;i<w->room_count;i++) {
        const CviiiPcRoom *r=&w->rooms[i];
        uint16_t cx,cy;
        if(r->stage==stage && cviii_pc_world_to_cell(r,wx,wy,&cx,&cy)) {
            if(found) return NULL; /* two rooms overlap; topology unresolved */
            found=r;
        }
    }
    return found;
}
bool cviii_pc_set_base(CviiiPcRoom *r,CviiiPcLayer l,uint16_t x,uint16_t y,uint16_t tile) {
    if(!in_room(r,x,y) || (unsigned)l>=3 || tile==CVIII_PC_TILE_UNSET) return false;
    cell(r,x,y)->base[l]=tile; return true;
}
bool cviii_pc_get_tile(const CviiiPcRoom *r,CviiiPcLayer l,uint16_t x,uint16_t y,uint16_t *out) {
    if(!in_room(r,x,y) || (unsigned)l>=3 || !out) return false;
    const CviiiPcTile *c=&r->cells[(size_t)y*r->width+x];
    *out=c->changed[l]==CVIII_PC_TILE_UNSET ? c->base[l] : c->changed[l];
    return true;
}
bool cviii_pc_change_tile(CviiiPcRoom *r,CviiiPcLayer l,uint16_t x,uint16_t y,
                          uint16_t tile,CviiiPcScope scope) {
    if(!in_room(r,x,y) || (unsigned)l>=3 || tile==CVIII_PC_TILE_UNSET ||
       !valid_scope(scope)) return false;
    CviiiPcTile *c=cell(r,x,y);
    if(tile==c->base[l]) {c->changed[l]=CVIII_PC_TILE_UNSET; c->scope[l]=0;}
    else {c->changed[l]=tile; c->scope[l]=(uint8_t)scope;}
    return true;
}
uint64_t cviii_pc_entity_id(uint8_t s,uint8_t a,uint8_t r,uint16_t slot) {
    return (UINT64_C(0xC8)<<56)|((uint64_t)s<<48)|((uint64_t)a<<40)|
           ((uint64_t)r<<32)|slot;
}
CviiiPcEntity *cviii_pc_entity(CviiiPcWorld *w,uint64_t id) {
    if(!w) return NULL;
    for(size_t i=0;i<w->entity_count;i++)
        if(w->entities[i].id==id) return &w->entities[i];
    return NULL;
}
bool cviii_pc_add_entity(CviiiPcWorld *w,uint8_t s,uint8_t a,uint8_t r,
                         uint16_t slot,uint8_t kind,int32_t x,int32_t y,uint8_t initial) {
    uint64_t id=cviii_pc_entity_id(s,a,r,slot);
    if(!w || !cviii_pc_room(w,s,a,r) || !kind || w->entity_count==CVIII_PC_MAX_ENTITIES ||
       cviii_pc_entity(w,id)) return false;
    w->entities[w->entity_count++]=(CviiiPcEntity){
        .id=id,.stage=s,.area=a,.room=r,.kind=kind,
        .local_x=x,.local_y=y,.initial_state=initial,.state=initial};
    return true;
}
bool cviii_pc_entity_world_position(const CviiiPcWorld *w,uint64_t id,
                                    int64_t *world_x,int64_t *world_y) {
    if(!w || !world_x || !world_y) return false;
    for(size_t i=0;i<w->entity_count;i++) {
        const CviiiPcEntity *e=&w->entities[i];
        if(e->id!=id) continue;
        const CviiiPcRoom *r=cviii_pc_room_const(w,e->stage,e->area,e->room);
        if(!r || !r->placed) return false;
        if((e->local_x>0 && r->world_x>INT64_MAX-e->local_x) ||
           (e->local_x<0 && r->world_x<INT64_MIN-(int64_t)e->local_x) ||
           (e->local_y>0 && r->world_y>INT64_MAX-e->local_y) ||
           (e->local_y<0 && r->world_y<INT64_MIN-(int64_t)e->local_y)) return false;
        *world_x=r->world_x+e->local_x; *world_y=r->world_y+e->local_y;
        return true;
    }
    return false;
}
bool cviii_pc_entity_state(CviiiPcWorld *w,uint64_t id,uint8_t state,CviiiPcScope scope) {
    CviiiPcEntity *e=cviii_pc_entity(w,id);
    if(!e || !valid_scope(scope)) return false;
    e->state=state; e->scope=(uint8_t)(state==e->initial_state?0:scope);
    return true;
}
void cviii_pc_enter_room(CviiiPcWorld *w,uint8_t s,uint8_t a,uint8_t id) {
    if(!w) return;
    if(w->has_visit && w->visit_stage==s && w->visit_area==a && w->visit_room==id) return;
    bool stage_change=w->has_visit && w->visit_stage!=s;
    for(size_t i=0;i<w->room_count;i++) {
        CviiiPcRoom *r=&w->rooms[i];
        bool was_room=w->has_visit && r->stage==w->visit_stage &&
            r->area==w->visit_area && r->room==w->visit_room;
        bool was_stage=stage_change && r->stage==w->visit_stage;
        if(!was_room && !was_stage) continue;
        for(size_t j=0;j<(size_t)r->width*r->height;j++) {
            CviiiPcTile *c=&r->cells[j];
            for(unsigned l=0;l<3;l++)
                if((was_room && c->scope[l]==CVIII_PC_VISIT) ||
                   (was_stage && c->scope[l]==CVIII_PC_STAGE)) {
                    c->changed[l]=CVIII_PC_TILE_UNSET; c->scope[l]=0;
                }
        }
    }
    for(size_t i=0;i<w->entity_count;i++) {
        CviiiPcEntity *e=&w->entities[i];
        bool was_room=w->has_visit && e->stage==w->visit_stage &&
            e->area==w->visit_area && e->room==w->visit_room;
        bool was_stage=stage_change && e->stage==w->visit_stage;
        if((was_room && e->scope==CVIII_PC_VISIT) ||
           (was_stage && e->scope==CVIII_PC_STAGE)) {
            e->state=e->initial_state; e->scope=0;
        }
    }
    w->visit_stage=s; w->visit_area=a; w->visit_room=id; w->has_visit=true;
}
