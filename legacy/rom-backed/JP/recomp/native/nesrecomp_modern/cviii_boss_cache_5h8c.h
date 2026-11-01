#ifndef CVIII_BOSS_CACHE_5H8C_H
#define CVIII_BOSS_CACHE_5H8C_H
#include <stdint.h>
/* Authentic previously observed, non-sprite native background pixels only.
 * Does not infer or synthesize unobserved portions of any boss scene. */
typedef struct {uint32_t color[512*480];uint8_t known[512*480];uint8_t stable[512*480];uint64_t last_frame;} CviiiBossCache5H8C;
/* New strict display APIs; legacy APIs remain for existing regression tests. */
unsigned cviii_boss_cache_match_stable_5h8d(const CviiiBossCache5H8C *c,
    const uint32_t native[256*240],const uint8_t oam[256],
    const int sx[240],const int sy[240],unsigned *matches);
unsigned cviii_boss_cache_paint_stable_5h8d(const CviiiBossCache5H8C *c,
    uint32_t *dest,const int sx[240],const int sy[240]);
void cviii_boss_cache_clear_5h8c(CviiiBossCache5H8C *c);
unsigned cviii_boss_cache_match_5h8c(const CviiiBossCache5H8C *c,
    const uint32_t native[256*240],const uint8_t oam[256],
    const int scroll_x[240],const int scroll_y[240],unsigned *matches);
void cviii_boss_cache_observe_5h8c(CviiiBossCache5H8C *c,uint64_t frame,
    const uint32_t native[256*240],const uint8_t oam[256],
    const int sx[240],const int sy[240]);
unsigned cviii_boss_cache_paint_5h8c(const CviiiBossCache5H8C *c,
    uint32_t *dest,const int sx[240],const int sy[240]);
#endif
