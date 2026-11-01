/* JP0653 synthetic C11 tests. Never embeds source game bytes. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_phrase_lookup_0653.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static unsigned word(const uint8_t *p, unsigned a) {
    a -= 0x8000u;
    return (unsigned)p[a] | ((unsigned)p[a+1u] << 8u);
}
static void putword(uint8_t *p, unsigned a, unsigned w) {
    a -= 0x8000u; p[a] = (uint8_t)w; p[a+1u] = (uint8_t)(w >> 8u);
}
int main(void) {
    uint8_t *bank = (uint8_t*)calloc(16384u,1u);
    CviiiPhraseLookup0653 out, keep;
    unsigned words[148], pos=0xA42Cu, unique[140], k, i, r=0;
    assert(bank);
    /* 140 genuine artificial phrase starts; 1323-byte pool. */
    for (i=0; i<140; ++i) {
        unsigned len = 9u + (i < 63u);
        unique[i] = pos;
        pos += len;
        bank[pos - 0x8000u - 1u] = 0xFFu;
    }
    assert(pos == 0xA957u);
    /* 8 pointers share one root; another two pointers share another. */
    for (i=0; i<148; ++i) {
        if (i>=104u && i<111u) words[i] = unique[r-1u];
        else if (i==128u) words[i] = unique[r-1u];
        else words[i] = unique[r++];
        putword(bank,0xA304u+2u*i,words[i]);
    }
    assert(r == 140u && words[0] == 0xA42Cu);
    for (k=0; k<2; ++k) for (i=0; i<256; ++i) {
        unsigned y=(i*2u)&255u, at=(k ? 0xA404u : 0xA304u)+y;
        int expected = !k || (i&127u)<20u;
        int status = cviii_phrase_lookup_0653(bank,16384,k,(uint8_t)i,&out);
        assert(status == expected);
        assert(out.final_y == y && out.original_raw_word == word(bank,at));
        assert(out.source_word_cpu_address == at);
        assert(out.final_z == ((out.original_raw_word >> 8u)==0u));
        assert(out.final_n == ((out.original_raw_word & 0x8000u)!=0u));
        assert(out.source_is_indexed_table == expected);
        if (expected) {
            assert(out.span_start == out.original_raw_word);
            assert(out.span_end_exclusive > out.span_start);
            assert(bank[out.span_end_exclusive-0x8000u-1u]==0xFFu);
        } else assert(!out.span_start && !out.span_end_exclusive);
    }
    keep=out;
    assert(cviii_phrase_lookup_0653(bank,16383,0,0,&out)==-1);
    assert(!memcmp(&keep,&out,sizeof out));
    assert(cviii_phrase_lookup_0653(bank,16384,2,0,&out)==-1);
    assert(!memcmp(&keep,&out,sizeof out));
    bank[0xA42Cu-0x8000u+9u] ^= 0x01u; /* damage first terminator */
    assert(cviii_phrase_lookup_0653(bank,16384,0,0,&out)==-1);
    assert(!memcmp(&keep,&out,sizeof out));
    bank[0xA42Cu-0x8000u+9u] ^= 0x01u;
    putword(bank,0xA304u+2u*2u,0xC000u);
    assert(cviii_phrase_lookup_0653(bank,16384,0,0,&out)==-1);
    assert(!memcmp(&keep,&out,sizeof out));
    free(bank);
    return 0;
}
