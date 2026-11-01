/* Pure host renderer regression: authentic CHR tile selection, clip, bounds. */
#include "cviii_audio_ui_0619.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define W 256u
#define H 240u
static uint32_t guarded[W*H+2u];
static unsigned char fake_chr[CVIII_CHR_PIXEL_BYTES];
static uint32_t at(unsigned x,unsigned y){return guarded[1+y*W+x];}
static void pattern(unsigned tile,unsigned pixel)
{
    for(unsigned y=1;y<7;y++)
        for(unsigned x=0;x<8;x++)
            fake_chr[tile*64u+y*8u+x]=(unsigned char)pixel;
}
int main(void)
{
    CviiiAudioLevels levels=CVIII_AUDIO_LEVELS_DEFAULT;
    CviiiChrIndex index={0};
    guarded[0]=0xA1B2C3D4u;
    guarded[W*H+1u]=0xE5F60718u;
    for(unsigned i=0;i<W*H;++i)guarded[1+i]=0xFF0000FFu;
    assert(!cviii_audio_ui_render(guarded+1,W,H,&index,&levels,0));
    assert(at(80,89)==0xFF0000FFu);  /* no unauthenticated fallback art */
    index.pixels=fake_chr;
    for(unsigned t=CVIII_AUDIO_UI_FONT_ZERO;t<CVIII_AUDIO_UI_FONT_ZERO+10u;t++) pattern(t,3);
    for(unsigned t=CVIII_AUDIO_UI_FONT_A;t<CVIII_AUDIO_UI_FONT_A+26u;t++) pattern(t,3);
    pattern(CVIII_AUDIO_UI_BAR_FULL,2);
    pattern(CVIII_AUDIO_UI_BAR_EMPTY,3);
    assert(!cviii_audio_ui_render(guarded+1,W+1u,H,&index,&levels,0));
    assert(!cviii_audio_ui_render(guarded+1,W,H,&index,&levels,3));
    assert(cviii_audio_ui_render(guarded+1,W,H,&index,&levels,0));
    assert(at(0,0)==0xFF0000FFu);
    assert(at(255,239)==0xFF0000FFu);
    assert(at(80,89)==0xFFFF882Cu);   /* health meter 100% */
    assert(at(16,89)==0xFFFF882Cu);   /* selected original menu font */
    assert(at(16,121)==0xFFF1EEEAu); /* unselected original menu font */
    assert(at(224,89)==0xFFFF882Cu);  /* original numeral 1 */
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,50));
    assert(cviii_audio_ui_render(guarded+1,W,H,&index,&levels,1));
    assert(at(80,89)==0xFFFF882Cu);
    assert(at(80+8*9,89)==0xFFF1EEEAu); /* first empty at 50% */
    assert(at(16,89)==0xFFF1EEEAu);
    assert(at(16,121)==0xFFFF882Cu);
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,1));
    assert(cviii_audio_ui_render(guarded+1,W,H,&index,&levels,0));
    assert(at(80,89)==0xFFFF882Cu);
    assert(at(81,89)==0xFFF1EEEAu); /* one-pixel rounded 1% fill */
    assert(cviii_audio_levels_set(&levels,CVIII_VOLUME_MASTER,0));
    assert(cviii_audio_ui_render(guarded+1,W,H,&index,&levels,0));
    assert(at(80,89)==0xFFF1EEEAu);
    assert(cviii_audio_ui_render_mode(guarded+1,W,H,&index,&levels,2,true));
    assert(at(64,182)==0xFFF1EEEAu); /* original-font pending-bus warning */
    assert(at(64,202)==0xFFF1EEEAu); /* original-font F6/ESC dismiss hint */
    assert(guarded[0]==0xA1B2C3D4u && guarded[W*H+1u]==0xE5F60718u);
    puts("PASS JP0619 health-meter art, authentic font slots, 1-percent fill and bounds");
    return 0;
}
