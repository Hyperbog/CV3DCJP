/* ROM-free: 426px preview center exactness, flags, and missing-pack fallback. */
#include "cviii_room_diag.h"
#include "cyc_core.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    const size_t n=256u*240u, big=426u*240u;
    uint32_t *original=malloc(n*sizeof(uint32_t));
    uint32_t *canvas=malloc(big*sizeof(uint32_t));
    assert(original&&canvas);
    for(size_t i=0;i<n;++i) original[i]=(uint32_t)(0xff000000u|i);
    cviii_room_diag_center_copy(canvas,426u,240u,original,UINT32_C(0xff080808));
    for(unsigned y=0;y<240u;++y) {
        for(unsigned x=0;x<85u;++x)
            assert(canvas[(size_t)y*426u+x]==UINT32_C(0xff080808));
        for(unsigned x=0;x<256u;++x)
            assert(canvas[(size_t)y*426u+85u+x]==original[(size_t)y*256u+x]);
        for(unsigned x=341u;x<426u;++x)
            assert(canvas[(size_t)y*426u+x]==UINT32_C(0xff080808));
    }
    assert(!cviii_room_diag_mode(-1));
    assert(!cviii_room_diag_mode(3));
    assert(cviii_room_diag_mode(2));
    assert(cviii_room_diag_get_mode()==2);
    int w=256,h=240;
    cviii_room_diag_frame_end();
    assert(cviii_room_diag_present(original,&w,&h)!=original);
    assert(w==426 && h==240);
    assert(cviii_room_diag_mode(0));
    free(original);free(canvas);
    puts("PASS 5F center exact and missing-resource fail-closed");
    return 0;
}
