/* Standalone ROM-free C11 PPU outcome fingerprint/sync test. */
#include "cviii_frame_evidence_0601.h"
#include <stdlib.h>
#include <string.h>

int main(void)
{
    CviiiFrameEvidence report = {0};
    CviiiChrFetchAudit *fetch = calloc(1, sizeof(*fetch));
    uint16_t *pixels = calloc(CVIII_FRAME_PIXELS, sizeof(*pixels));
    uint8_t *opaque = calloc(CVIII_FRAME_PIXELS, 1);
    uint8_t lines[CVIII_FRAME_LINES_BYTES] = {0};
    if (!fetch || !pixels || !opaque) return 1;
    report.output = tmpfile();
    if (!report.output) return 2;
    fetch->frames = 1;
    fetch->active = 1;
    fetch->total_reads = 5;
    fetch->total_bad = 1;
    fetch->total_invalid = 0;
    fetch->lines[15].bank_changes[1] = 2;
    pixels[0] = 0x1ff;
    opaque[0] = 1;
    lines[4] = 0x18;
    if (!cviii_frame_evidence_sample(&report, fetch, pixels, opaque, lines)) return 3;
    rewind(report.output);
    char entry[256];
    if (!fgets(entry, sizeof(entry), report.output) ||
        !strstr(entry, "1,5,1,0,2,") ||
        !strstr(entry, ",1,1,1")) return 4;
    uint64_t original_samples = report.samples;
    fetch->active = 0;
    if (cviii_frame_evidence_sample(&report, fetch, pixels, opaque, lines) ||
        report.samples != original_samples) return 5;
    fetch->active = 1;
    pixels[0] = 0x200; /* reject non-NES palette + emphasis index */
    if (cviii_frame_evidence_sample(&report, fetch, pixels, opaque, lines) ||
        report.input_failures != 1) return 6;
    pixels[0] = 0x1ff;
    opaque[0] = 2;
    if (cviii_frame_evidence_sample(&report, fetch, pixels, opaque, lines) ||
        report.input_failures != 2) return 7;
    cviii_frame_evidence_close(&report);
    free(pixels); free(opaque); free(fetch);
    puts("PASS JP0601 sampled original-PPU fingerprint/CHR-fetch alignment (ROM-free)");
    return 0;
}
