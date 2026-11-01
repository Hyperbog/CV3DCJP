#include "cviii_camera_track_5h7.h"
#include <string.h>
#include <limits.h>
void cviii_camera_track_reset_5h7(CviiiCameraTrack5H7 *t)
{
    if(t)memset(t,0,sizeof *t);
}
static unsigned unique(int32_t out[3],unsigned n,int32_t value,int length)
{
    if(value<0 || value>=length)return n;
    for(unsigned i=0;i<n;i++)if(out[i]==value)return n;
    if(n<3)out[n++]=value;
    return n;
}
unsigned cviii_camera_track_propose_5h7(CviiiCameraTrack5H7 *t,
    uint64_t frame,unsigned stage,unsigned area,unsigned room,
    int scroll,unsigned room_pixels,int32_t out[3])
{
    if(!t||!out||scroll<0||scroll>=512||room_pixels==0||
       room_pixels>INT32_MAX)return 0;
    /* No speculative world topology or tracking on vertical rooms. */
    if(stage!=0||area!=0||room!=0){cviii_camera_track_reset_5h7(t);return 0;}
    if(!t->primed||t->stage!=stage||t->area!=area||t->room!=room||
       frame<t->frame||frame>t->frame+1) {
        cviii_camera_track_reset_5h7(t);
        t->primed=true;t->predicted=scroll;
    } else if(frame>t->frame) {
        int delta=(scroll-t->last_scroll+512)%512;
        if(delta>256)delta-=512;
        /* Reset at scene changes/teleports, not on ordinary 511->0 wrap. */
        if(delta>32||delta< -32||
           (int64_t)t->predicted+delta<0||
           (int64_t)t->predicted+delta>=room_pixels) {
            t->predicted=scroll;t->confirmed=false;
        } else t->predicted+=delta;
    }
    t->frame=frame;t->last_scroll=scroll;
    t->stage=stage;t->area=area;t->room=room;
    unsigned n=0;
    n=unique(out,n,t->predicted,(int)room_pixels);
    n=unique(out,n,scroll,(int)room_pixels);
    /* If capture begins after a wrap, probe the other *same-room* page.
       Repetition remains only a hypothesis until 96% pixel parity. */
    n=unique(out,n,scroll+512,(int)room_pixels);
    return n;
}
bool cviii_camera_track_allowed_5h7(const CviiiCameraTrack5H7 *t,
                                    int32_t candidate)
{
    if(!t||!t->primed)return false;
    /* A real reset allows re-acquisition; a smooth scroll requires locality. */
    if(!t->confirmed)return true;
    int64_t difference=(int64_t)candidate-t->predicted;
    return difference>=-32 && difference<=32;
}
void cviii_camera_track_confirm_5h7(CviiiCameraTrack5H7 *t,int32_t pixel)
{
    if(t&&t->primed&&pixel>=0){
        t->predicted=pixel;t->verified=pixel;t->confirmed=true;
    }
}
