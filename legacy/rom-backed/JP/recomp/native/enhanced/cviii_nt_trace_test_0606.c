/* JP0606 synthetic read-only PPU tile/attribute fetch contract. */
#include "cviii_nt_trace_0606.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CviiiNtHook live;
void cviii_nt_set_hook(CviiiNtHook fn) { live = fn; }

int main(void)
{
    CviiiNtTrace *t = (CviiiNtTrace *)calloc(1, sizeof(*t));
    if (!t) return 1;
    FILE *out = tmpfile();
    if (!out) return 2;
    t->output = out;
    cviii_nt_begin(t, false, 1u);
    if (live || t->active) return 3;
    cviii_nt_begin(t, true, 61u);
    if (!live || !t->active || t->frames != 61u) return 4;
    live(0x2000u, 0x34u, 0u, 40u, 8u);
    live(0x23c0u, 0x55u, 0u, 40u, 12u);
    live(0x27c0u, 0x66u, 1u, 40u, 16u);
    live(0x0001u, 0x77u, 0u, 40u, 20u);
    live(0x23c1u, 0x99u, 0u, 40u, 341u);
    if (t->total_tiles != 1 || t->total_attrs != 2 ||
        t->total_rom != 1 || t->total_invalid != 2 ||
        t->lines[40].ciram != 2 || t->lines[40].chr_rom != 1 ||
        t->lines[40].first_tile != 0x34 || t->lines[40].first_attr != 0x55)
        return 5;
    cviii_nt_end(t);
    if (live || t->active || t->sample_frames != 1) return 6;
    rewind(out);
    char line[300];
    if (!fgets(line, sizeof(line), out) ||
        !strstr(line, "61,40,1,2,2,1,2,52,85,")) return 7;
    cviii_nt_begin(t, false, 62u);
    if (live || t->active) return 8;
    cviii_nt_begin(t, true, 121u);
    if (!live) return 9;
    cviii_nt_end(t);
    if (live || t->sample_frames != 2) return 10;
    rewind(out);
    int found_empty = 0;
    while (fgets(line, sizeof(line), out))
        if (strstr(line, "121,0,0,0,0,0,0,-1,-1,")) found_empty = 1;
    if (!found_empty) return 11;
    fclose(out);
    t->output = NULL;
    free(t);
    puts("PASS JP0606 read-only PPU nametable/attribute and source audit");
    return 0;
}
