/* Versioned owner-local state snapshot. No ROM bytes, no raw NES savestate.
 * Loads transactionally: invalid files never overwrite live world state. */
#include "cviii_pc_world.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

bool cviii_pc_save(const CviiiPcWorld *w,const char *filename) {
    if(!w || !filename) return false;
    FILE *f=fopen(filename,"wb");
    if(!f) return false;
    int error=fprintf(f,"CVIII-PCSTATE-1 %016" PRIx64 " %u %u %u %u\n",
        w->resource_signature,w->has_visit?1u:0u,
        w->visit_stage,w->visit_area,w->visit_room)<0;
    for(size_t i=0;i<w->room_count && !error;i++) {
        const CviiiPcRoom *r=&w->rooms[i];
        for(uint32_t y=0;y<r->height && !error;y++)for(uint32_t x=0;x<r->width && !error;x++) {
            const CviiiPcTile *c=&r->cells[(size_t)y*r->width+x];
            for(unsigned l=0;l<3;l++) if(c->scope[l]>=CVIII_PC_STAGE &&
                    c->changed[l]!=CVIII_PC_TILE_UNSET)
                error|=fprintf(f,"T %u %u %u %u %u %u %u %u\n",
                    r->stage,r->area,r->room,x,y,l,c->changed[l],c->scope[l])<0;
        }
    }
    for(size_t i=0;i<w->entity_count && !error;i++) {
        const CviiiPcEntity *e=&w->entities[i];
        if(e->scope>=CVIII_PC_STAGE)
            error|=fprintf(f,"E %016" PRIx64 " %u %u\n",e->id,e->state,e->scope)<0;
    }
    if(fclose(f)!=0) error=1;
    return !error;
}
static CviiiPcWorld *fresh_like(const CviiiPcWorld *w) {
    CviiiPcWorld *t=cviii_pc_new(w->resource_signature);
    if(!t) return NULL;
    for(size_t i=0;i<w->room_count;i++) {
        const CviiiPcRoom *r=&w->rooms[i];
        if(!cviii_pc_add_room(t,r->stage,r->area,r->room,r->width,r->height)) goto fail;
        CviiiPcRoom *n=&t->rooms[t->room_count-1];
        for(size_t j=0;j<(size_t)r->width*r->height;j++)
            memcpy(n->cells[j].base,r->cells[j].base,sizeof(n->cells[j].base));
        if(r->placed && !cviii_pc_place_room(t,r->stage,r->area,r->room,r->world_x,r->world_y)) goto fail;
    }
    memcpy(t->entities,w->entities,w->entity_count*sizeof(w->entities[0]));
    t->entity_count=w->entity_count;
    for(size_t i=0;i<t->entity_count;i++) {
        t->entities[i].state=t->entities[i].initial_state;
        t->entities[i].scope=0;
    }
    return t;
fail:
    cviii_pc_free(t);
    return NULL;
}
static bool trailing_space_only(const char *line,int offset) {
    for(const char *p=line+offset;*p;p++) if(!isspace((unsigned char)*p)) return false;
    return true;
}
bool cviii_pc_load(CviiiPcWorld *w,const char *filename) {
    if(!w || !filename) return false;
    FILE *f=fopen(filename,"rb"); if(!f) return false;
    char line[256]; int n=0; int ok=0;
    unsigned has,s,a,r; uint64_t sig=0;
    if(!fgets(line,sizeof(line),f) || !strchr(line,'\n') ||
       sscanf(line,"CVIII-PCSTATE-1 %" SCNx64 " %u %u %u %u %n",
           &sig,&has,&s,&a,&r,&n)!=5 || !trailing_space_only(line,n) ||
       sig!=w->resource_signature || has>1 || s>255 || a>255 || r>255) {
        fclose(f); return false;
    }
    CviiiPcWorld *t=fresh_like(w);
    if(!t) {fclose(f);return false;}
    t->has_visit=has!=0;
    t->visit_stage=(uint8_t)s; t->visit_area=(uint8_t)a; t->visit_room=(uint8_t)r;
    ok=1; size_t records=0;
    while(ok && fgets(line,sizeof(line),f)) {
        if(!strchr(line,'\n') || ++records>1000000) {ok=0;break;}
        n=0;
        if(line[0]=='T' && line[1]==' ') {
            unsigned stage,area,room,x,y,layer,tile,scope;
            if(sscanf(line,"T %u %u %u %u %u %u %u %u %n",
                &stage,&area,&room,&x,&y,&layer,&tile,&scope,&n)!=8 ||
                !trailing_space_only(line,n) || stage>255 || area>255 || room>255 ||
                x>UINT16_MAX || y>UINT16_MAX || layer>=3 || tile>=CVIII_PC_TILE_UNSET ||
                (scope!=CVIII_PC_STAGE && scope!=CVIII_PC_SESSION)) {ok=0;break;}
            CviiiPcRoom *rr=cviii_pc_room(t,(uint8_t)stage,(uint8_t)area,(uint8_t)room);
            if(!rr || !cviii_pc_change_tile(rr,(CviiiPcLayer)layer,
                (uint16_t)x,(uint16_t)y,(uint16_t)tile,(CviiiPcScope)scope)) ok=0;
        } else if(line[0]=='E' && line[1]==' ') {
            uint64_t id; unsigned state,scope;
            if(sscanf(line,"E %" SCNx64 " %u %u %n",&id,&state,&scope,&n)!=3 ||
                !trailing_space_only(line,n) || state>255 ||
                (scope!=CVIII_PC_STAGE && scope!=CVIII_PC_SESSION) ||
                !cviii_pc_entity_state(t,id,(uint8_t)state,(CviiiPcScope)scope)) ok=0;
        } else ok=0;
    }
    int stream_error=ferror(f);
    if(fclose(f)!=0) stream_error=1;
    if(stream_error) ok=0;
    if(ok) {
        for(size_t i=0;i<w->room_count;i++) {
            free(w->rooms[i].cells);
            w->rooms[i].cells=t->rooms[i].cells;
            t->rooms[i].cells=NULL;
        }
        memcpy(w->entities,t->entities,t->entity_count*sizeof(t->entities[0]));
        w->has_visit=t->has_visit;
        w->visit_stage=t->visit_stage;
        w->visit_area=t->visit_area;
        w->visit_room=t->visit_room;
    }
    cviii_pc_free(t);
    return ok!=0;
}
