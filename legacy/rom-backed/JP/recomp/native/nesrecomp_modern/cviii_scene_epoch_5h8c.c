#include "cviii_scene_epoch_5h8c.h"
#include <string.h>

void cviii_scene_epoch_reset_5h8c(CviiiSceneEpoch5H8C *s)
{
    if(s)memset(s,0,sizeof *s);
}
/* A major rewrite is evidence for a new scene epoch, not proof of geometry.
 * Adjacent large rewrites are grouped into the same provisional loading epoch.
 * CHR and palette changes produce revisions; they cannot authorize scenery. */
bool cviii_scene_epoch_observe_5h8c(CviiiSceneEpoch5H8C *s,
    uint64_t frame,uint8_t stage,uint8_t area,uint8_t room,
    const uint8_t *ciram,size_t ciram_len,
    const uint8_t palette[32],uint32_t mapped_chr_hash,
    CviiiSceneEvent5H8C *out)
{
    if(!s||!out||!ciram||!palette||ciram_len!=2048u || !frame)
        return false;
    CviiiSceneEvent5H8C e={0};
    bool new_room=!s->initialized||s->stage!=stage||s->area!=area||s->room!=room;
    if(new_room) {
        e.flags|=CVIII_EPOCH_ROOM;
        s->epoch=1; s->revision=0; s->last_major_frame=0;
    } else {
        if(frame<=s->frame || frame>s->frame+1u) {
            e.flags|=CVIII_EPOCH_DISCONTINUITY;
        }
        for(unsigned i=0;i<2048u;i++)
            e.ciram_changed+=(ciram[i]!=s->ciram[i]);
        if(e.ciram_changed>=128u) {
            e.flags|=CVIII_EPOCH_MAJOR_CIRAM;
            if(!s->last_major_frame || frame>s->last_major_frame+2u)
                s->epoch++;
            s->last_major_frame=frame;
        }
        if(memcmp(palette,s->palette,32u)) {
            e.flags|=CVIII_EPOCH_PALETTE;
            s->revision++;
        }
        if(mapped_chr_hash!=s->chr_hash) {
            e.flags|=CVIII_EPOCH_CHR;
            s->revision++;
        }
    }
    memcpy(s->ciram,ciram,2048u);memcpy(s->palette,palette,32u);
    s->chr_hash=mapped_chr_hash;
    s->frame=frame;s->stage=stage;s->area=area;s->room=room;s->initialized=1;
    e.epoch=s->epoch;e.revision=s->revision;
    *out=e;
    return true;
}
