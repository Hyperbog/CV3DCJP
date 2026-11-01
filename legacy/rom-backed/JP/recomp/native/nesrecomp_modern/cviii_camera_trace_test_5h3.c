#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_camera_trace_5h3.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    CviiiCameraTrace5H3 trace={0};
    const char *path="cviii_camera_trace_5h3_test.csv";
    assert(cviii_camera_trace_open_5h3(&trace,path));
    assert(!cviii_camera_trace_open_5h3(&trace,path));
    CviiiCameraObservation5H3 r={0};
    r.stage=0;r.room=0;r.camera_ram=255;r.qualified=1;
    r.camera_x=255;r.top=32;r.tested=110;r.matched=108;
    r.world_placed=1;r.source=5;
    assert(cviii_camera_trace_append_5h3(&trace,&r));
    assert(!cviii_camera_trace_append_5h3(&trace,&r));
    r.frame=1;r.camera_ram=256;r.camera_x=256;
    assert(cviii_camera_trace_append_5h3(&trace,&r));
    r.frame=2;r.area=1;r.camera_ram=0;r.camera_x=0;
    assert(cviii_camera_trace_append_5h3(&trace,&r));
    assert(trace.samples==3 && trace.transitions==1 && trace.qualified==3);
    r.frame=3;r.matched=111;
    assert(!cviii_camera_trace_append_5h3(&trace,&r));
    cviii_camera_trace_close_5h3(&trace);
    FILE *f=fopen(path,"rb");assert(f);
    char line[512];int rows=0,transitions=0;
    while(fgets(line,sizeof line,f)){
       if(rows>0){
           unsigned long long frame;
           unsigned s,a,room,transition;
           assert(sscanf(line,"%llu,%u,%u,%u,%u",&frame,&s,&a,&room,&transition)==5);
           transitions+=(int)transition;
       }
       rows++;
    }
    fclose(f);remove(path);
    assert(rows==4 && transitions==1);
    puts("PASS 5H3 camera observations, transition detection, duplicate-frame guard");
    return 0;
}
