#include "cviii_camera_track_5h7.h"
#include <assert.h>
#include <stdio.h>
static int contains(const int32_t *a,unsigned n,int32_t x)
{
    for(unsigned i=0;i<n;i++)if(a[i]==x)return 1;
    return 0;
}
int main(void)
{
    CviiiCameraTrack5H7 t={0};int32_t a[3];unsigned n;
    n=cviii_camera_track_propose_5h7(&t,1,0,0,0,0,768,a);
    assert(contains(a,n,0)&&contains(a,n,512));
    cviii_camera_track_confirm_5h7(&t,0);
    assert(cviii_camera_track_allowed_5h7(&t,0));
    assert(!cviii_camera_track_allowed_5h7(&t,512));
    for(int i=1;i<=511;i++) {
        n=cviii_camera_track_propose_5h7(&t,(uint64_t)i+1,0,0,0,i,768,a);
        assert(contains(a,n,i));
        assert(cviii_camera_track_allowed_5h7(&t,i));
        cviii_camera_track_confirm_5h7(&t,i);
    }
    n=cviii_camera_track_propose_5h7(&t,513,0,0,0,0,768,a);
    assert(contains(a,n,512));
    assert(!cviii_camera_track_allowed_5h7(&t,0));
    cviii_camera_track_confirm_5h7(&t,512);
    n=cviii_camera_track_propose_5h7(&t,514,0,0,0,1,768,a);
    assert(contains(a,n,513)&&t.predicted==513);
    /* No double-count when presentation is invoked twice in one frame. */
    n=cviii_camera_track_propose_5h7(&t,514,0,0,0,1,768,a);
    assert(contains(a,n,513)&&t.predicted==513);
    /* A discontinuity permits reacquisition, but NOT automatic painting. */
    n=cviii_camera_track_propose_5h7(&t,515,0,0,0,80,768,a);
    assert(!t.confirmed&&contains(a,n,80));
    cviii_camera_track_confirm_5h7(&t,80);
    n=cviii_camera_track_propose_5h7(&t,516,0,1,0,80,768,a);
    assert(n==0&&!t.primed);
    n=cviii_camera_track_propose_5h7(&t,517,0,0,0,-1,768,a);
    assert(n==0);
    n=cviii_camera_track_propose_5h7(&t,518,0,0,0,0,768,a);
    assert(contains(a,n,0));
    cviii_camera_track_confirm_5h7(&t,0);
    n=cviii_camera_track_propose_5h7(&t,521,0,0,0,1,768,a);
    assert(!t.confirmed&&contains(a,n,1)); /* missing frames reset */
    puts("PASS: 5H-7 scroll unwrap, reacquisition, wrap, frame/room resets");
    return 0;
}
