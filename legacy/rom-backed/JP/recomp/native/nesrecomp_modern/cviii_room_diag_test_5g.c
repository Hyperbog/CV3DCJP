/* Phase 5G ROM-free screen-width + authentic-center + PPU-page sample gate. */
#include "cviii_room_diag.h"
#include "cyc_core.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    const size_t n=256u*240u;
    uint32_t *original=(uint32_t *)malloc(n*sizeof(uint32_t));
    uint32_t *canvas=(uint32_t *)malloc(426u*240u*sizeof(uint32_t));
    assert(original && canvas);
    for(size_t i=0;i<n;++i) original[i]=UINT32_C(0xff123400)+(uint32_t)(i&255u);
    cviii_room_diag_center_copy(canvas,426u,240u,original,UINT32_C(0xff090909));
    assert(canvas[0u]==UINT32_C(0xff090909));
    for(unsigned y=0;y<240u;++y)for(unsigned x=0;x<256u;++x)
        assert(canvas[(size_t)y*426u+85u+x]==original[(size_t)y*256u+x]);
    assert(!cviii_room_diag_mode(3));
    assert(!cviii_room_diag_source("maybe"));
    assert(cviii_room_diag_source("ppu"));
    assert(cviii_room_diag_get_source()[0]=='p');
    assert(cviii_room_diag_mode(2));
    cviii_room_diag_frame_end(); /* intentionally no authenticated caches */
    int w=256,h=240;
    const uint32_t *image=cviii_room_diag_present(original,&w,&h);
    assert(image!=original && w==426 && h==240);
    for(unsigned y=0;y<240u;y++) {
        for(unsigned x=0;x<256u;x++)
            assert(image[(size_t)y*426u+85u+x]==original[(size_t)y*256u+x]);
    }
    /* Synthetic mapped nametable gives non-matte sampled background outside HUD. */
    assert(image[100u*426u+0u]==UINT32_C(0xff000000));
    assert(image[100u*426u+84u]==UINT32_C(0xff000001));
    assert(image[100u*426u+425u]==UINT32_C(0xff000000));
    assert(image[0u]==UINT32_C(0xff000000)); /* Side HUD remains empty */
    assert(cviii_room_diag_mode(1));
    w=256;h=240;
    image=cviii_room_diag_present(original,&w,&h);
    assert(image!=original && w==512 && h==240);
    for(unsigned y=0;y<240u;y++)for(unsigned x=0;x<256u;x++)
        assert(image[(size_t)y*512u+x]==original[(size_t)y*256u+x]);
    assert(image[120u*512u+300u]==UINT32_C(0xff000001));
    assert(cviii_room_diag_source("room")); /* strict room source: matte, but still widescreen */
    assert(cviii_room_diag_mode(2)); w=256;h=240;
    image=cviii_room_diag_present(original,&w,&h);
    assert(image!=original && w==426 && h==240);
    assert(image[100u*426u+0u]==UINT32_C(0xff000000));
    assert(cviii_room_diag_mode(0));w=256;h=240;
    assert(cviii_room_diag_present(original,&w,&h)==original && w==256);
    free(original); free(canvas);
    puts("PASS 5G 426/512 viewport, fallback margins, original-center preservation");
    return 0;
}
