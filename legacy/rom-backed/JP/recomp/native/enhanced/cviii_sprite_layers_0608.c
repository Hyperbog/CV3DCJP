/* JP0608: one-frame PC-only sprite continuity; no machine-state changes.
 * This is an experimental visual improvement, not actor identity recovery.
 * Per-sprite pixels are captured during the host's frame_end callback.
 */
#include "cviii_sprite_layers_0608.h"
#include <stdlib.h>
#include <string.h>
static bool near(int a, int b, int radius) { return abs(a - b) <= radius; }
/* JP0645: original PRG26 rotates first OAM write by $44 (17 slots)
 * and advances each logical emission by $C4 (-15 slots). The inverse of
 * -15 mod 64 is 17, so an emission index is stable across that rotation.
 * OAM phase is the byte-valued original zero-page $22, not an emulated
 * hardware priority change; this is presentation-only association. */
static unsigned emission_index(unsigned slot, uint8_t phase)
{
    return (((slot - ((unsigned)phase >> 2)) & 63u) * 17u) & 63u;
}
static unsigned emission_slot(unsigned emission, uint8_t phase)
{
    return (((unsigned)phase >> 2) + emission * 49u) & 63u;
}
static bool same_logical_piece(const CviiiLayerSprite *a,
                               const CviiiLayerSprite *b)
{
    /* Allow tile animation and flip changes without creating a ghost.
     * Require proximity, same sprite height and same palette group.
     * Motion/priority flags and the 2bpp tile index are not identities. */
    return b->valid && a->height == b->height &&
           (a->attr & 3u) == (b->attr & 3u) &&
           near(a->x,b->x,4) && near(a->y,b->y,4);
}
static bool matches(const CviiiLayerSprite *a, const CviiiLayerSprite *b,
                    unsigned old_slot, unsigned new_slot)
{
    if (!b->valid || !near(a->x,b->x,4) || !near(a->y,b->y,4) ||
        (a->attr & 0xC3) != (b->attr & 0xC3) || a->height != b->height)
        return false;
    return a->tile == b->tile ||
           (old_slot == new_slot && near(a->x,b->x,1) && near(a->y,b->y,1));
}
void cviii_layers_reset(CviiiSpriteLayers *layers)
{
    if (layers) memset(layers,0,sizeof(*layers));
}
static void sample(CviiiLayerSprite *s, const uint8_t *oam,
                   const uint8_t mask[CVIII_LAYER_H],
                   const uint8_t tall[CVIII_LAYER_H],
                   const uint16_t tables[CVIII_LAYER_H], const CviiiLayerFns *f)
{
    if (oam[0] >= 0xEF) return;
    unsigned y = (unsigned)oam[0]+1;
    if (y >= CVIII_LAYER_H || !(mask[y]&0x10)) return;
    s->valid=1; s->x=oam[3]; s->y=(uint8_t)y;
    s->tile=oam[1]; s->attr=oam[2]; s->height=tall[y]?16:8;
    for (unsigned row=0; row<s->height; ++row) {
        unsigned rr=(s->attr&0x80)?s->height-1-row:row;
        uint16_t base=s->height==16
          ? (uint16_t)(((s->tile&1u)?0x1000u:0u)+(s->tile&0xFEu)*16u+
                       (rr>=8u?16u:0u))
          : (uint16_t)(tables[y]+(unsigned)s->tile*16u);
        uint8_t lo=f->read_chr(f->user,(uint16_t)(base+(rr&7u)));
        uint8_t hi=f->read_chr(f->user,(uint16_t)(base+(rr&7u)+8u));
        for (unsigned x=0; x<8; ++x) {
            unsigned bit=(s->attr&0x40)?x:7u-x;
            s->pixels[row*8+x]=(uint8_t)(((lo>>bit)&1u) |
                                     (((hi>>bit)&1u)<<1u));
        }
    }
}
void cviii_layers_capture(CviiiSpriteLayers *s, const uint8_t oam[256],
                          uint8_t oam_start_phase,
                          const uint8_t mask[CVIII_LAYER_H],
                          const uint8_t tall[CVIII_LAYER_H],
                          const uint16_t tables[CVIII_LAYER_H],
                          const CviiiLayerFns *f)
{
    if (!s || !oam || !mask || !tall || !tables || !f || !f->read_chr) return;
    s->prior_phase=s->phase;
    s->phase=oam_start_phase;
    CviiiLayerSprite previous[CVIII_LAYER_SLOTS];
    memcpy(previous,s->current,sizeof(previous));
    memset(s->current,0,sizeof(s->current));
    memset(s->held,0,sizeof(s->held));
    s->held_now=0;
    unsigned present=0,old_present=0;
    for(unsigned i=0;i<CVIII_LAYER_SLOTS;++i) {
        sample(&s->current[i],oam+i*4,mask,tall,tables,f);
        present+=s->current[i].valid?1u:0u;
        old_present+=previous[i].valid?1u:0u;
    }
    /* Major scene transitions, menus and game-over must not leave ghosts. */
    bool scene_change=old_present>=8u && present*2u<old_present;
    if(s->primed && !scene_change) {
        /* Distinct preceding sprite pieces may overlap; do not let one
         * present sprite consume several old identities. Prioritize
         * original logical emission positions before tile heuristics.
         * Non-word-aligned $22 values cannot be trusted as an OAM phase. */
        bool previous_paired[CVIII_LAYER_SLOTS]={false};
        bool current_used[CVIII_LAYER_SLOTS]={false};
        if(((s->phase | s->prior_phase) & 3u) == 0u) {
            for(unsigned i=0;i<CVIII_LAYER_SLOTS;++i) {
                if(!previous[i].valid)continue;
                const unsigned emission=emission_index(i,s->prior_phase);
                const unsigned j=emission_slot(emission,s->phase);
                if(!current_used[j] &&
                   same_logical_piece(&previous[i],&s->current[j])) {
                    previous_paired[i]=true;
                    current_used[j]=true;
                }
            }
        }
        /* Conservative fallback for non-rotating and unusual sequences:
         * exact tile/attributes and spatial overlap, one-to-one only. */
        for(unsigned i=0;i<CVIII_LAYER_SLOTS;++i) {
            if(!previous[i].valid || previous_paired[i])continue;
            unsigned best=CVIII_LAYER_SLOTS;
            unsigned distance=100u;
            for(unsigned j=0;j<CVIII_LAYER_SLOTS;++j) {
                if(current_used[j] || !matches(&previous[i],&s->current[j],i,j))
                    continue;
                const unsigned dx=(unsigned)abs((int)previous[i].x-(int)s->current[j].x);
                const unsigned dy=(unsigned)abs((int)previous[i].y-(int)s->current[j].y);
                const unsigned d=dx+dy;
                if(d<distance) {distance=d;best=j;}
            }
            if(best<CVIII_LAYER_SLOTS) {
                current_used[best]=true;
                previous_paired[i]=true;
            }
        }
        for(unsigned i=0;i<CVIII_LAYER_SLOTS;++i) {
            if(previous[i].valid && !previous_paired[i]) {
                s->held[i]=previous[i];
                ++s->held_now;
            }
        }
    }
    s->primed=true; ++s->frames; s->total_held+=s->held_now;
}
static void paint(const CviiiLayerSprite *s,uint32_t *frame,const uint8_t *bg,
                  const uint8_t mask[CVIII_LAYER_H],const CviiiLayerFns *f)
{
    if(!s->valid)return;
    for(unsigned row=0;row<s->height;++row){
        unsigned y=(unsigned)s->y+row;
        if(y>=CVIII_LAYER_H || !(mask[y]&0x10))continue;
        for(unsigned col=0;col<8;++col){
            unsigned x=(unsigned)s->x+col;
            if(x>=CVIII_LAYER_W || (x<8 && !(mask[y]&0x04)))continue;
            unsigned color=s->pixels[row*8+col];
            if(!color)continue;
            size_t at=(size_t)y*CVIII_LAYER_W+x;
            if((s->attr&0x20) && bg && bg[at])continue;
            frame[at]=f->color(f->user,16u+(s->attr&3u)*4u+color);
        }
    }
}
void cviii_layers_draw(const CviiiSpriteLayers *s,uint32_t *frame,
                       const uint8_t bg[CVIII_LAYER_W * CVIII_LAYER_H],
                       const uint8_t mask[CVIII_LAYER_H],
                       const CviiiLayerFns *f)
{
    if(!s || !frame || !mask || !f || !f->color)return;
    /* JP0609: PRG26:$8001 rotates its OAM byte offset by +$44
     * (17 hardware slots) each frame. Each emitted sprite then advances
     * by +$C4 bytes (-15 slots). NES priority by raw OAM slot therefore
     * visibly swaps overlapped actors as that offset rotates.
     *
     * Reconstruct the STABLE logical emission order. Lower emission
     * indices were written earlier by the original per-object builder
     * and remain in front when overlap occurs. Never alter the PPU's
     * real OAM, sprite-zero, overflow or game collision flags.
     *
     * Paint previous-frame rescued pieces first under CURRENT sprites:
     * an older ghost can never obscure a live sprite in this mode.
     */
    for(int emission=CVIII_LAYER_SLOTS-1;emission>=0;--emission) {
        uint8_t byte_offset=(uint8_t)(s->prior_phase+
                                      (unsigned)emission*0xC4u);
        unsigned slot=(byte_offset>>2)&63u;
        paint(&s->held[slot],frame,bg,mask,f);
    }
    for(int emission=CVIII_LAYER_SLOTS-1;emission>=0;--emission) {
        uint8_t byte_offset=(uint8_t)(s->phase+
                                      (unsigned)emission*0xC4u);
        unsigned slot=(byte_offset>>2)&63u;
        paint(&s->current[slot],frame,bg,mask,f);
    }
}
