/* ROM-free C11 test of the original-byte fetch parity and bank-switch log.
 * A hook mock replaces the absent external mapper in this standalone test.
 */
#include "cviii_chr_fetch_0600.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CviiiChrFetchHook registered_hook;
void cviii_chr_fetch_set_hook(CviiiChrFetchHook hook) { registered_hook=hook; }

int main(void)
{
    CviiiChrIndex asset = {0};
    asset.allocation = calloc(1, CVIII_CHR_RAW_BYTES + CVIII_CHR_PIXEL_BYTES);
    if (!asset.allocation) return 1;
    asset.raw = asset.allocation;
    asset.pixels = asset.allocation + CVIII_CHR_RAW_BYTES;
    CviiiChrFetchAudit *audit = (CviiiChrFetchAudit *)calloc(1, sizeof(*audit));
    if (!audit) return 2;
    FILE *tmp = tmpfile();
    if (!tmp) return 3;
    audit->output = tmp;
    cviii_chr_fetch_frame_begin(audit, &asset);
    if (!registered_hook || !audit->active) return 4;
    registered_hook(0x0000, 1u*1024, 0, 40, 1);
    registered_hook(0x0000, 2u*1024, 0, 40, 2);
    registered_hook(0x0000, 2u*1024, 1, 40, 3);
    registered_hook(0x0400, 3u*1024, 0, 40, 4);
    registered_hook(0x1fff, CVIII_CHR_RAW_BYTES+1u, 0, 40, 5);
    if (audit->total_reads != 5 || audit->total_bad != 1 ||
        audit->total_invalid != 1 || audit->lines[40].reads != 4 ||
        audit->lines[40].bank_changes[0] != 1 ||
        audit->lines[40].first_bank[0] != 1 ||
        audit->lines[40].last_bank[0] != 2 ||
        audit->lines[40].first_bank[1] != 3) return 5;
    cviii_chr_fetch_frame_end(audit);
    if (registered_hook || audit->active || audit->sample_frames != 1) return 6;
    rewind(tmp);
    char line[512];
    if (!fgets(line, sizeof(line), tmp) ||
        !strstr(line, "40,4,1,1,1,2,1,3,3,0")) return 7;
    for (unsigned i=0;i<59;++i) {
        cviii_chr_fetch_frame_begin(audit,&asset);
        if (registered_hook || audit->active) return 8;
    }
    cviii_chr_fetch_frame_begin(audit,&asset);
    if (!registered_hook) return 9;
    cviii_chr_fetch_frame_end(audit);
    if (audit->sample_frames != 2) return 10;
    fclose(tmp); audit->output = NULL;
    free(audit);
    free(asset.allocation);
    puts("PASS JP0600 ROM-free PPU CHR fetch parity, mid-line bank changes and sampling");
    return 0;
}
