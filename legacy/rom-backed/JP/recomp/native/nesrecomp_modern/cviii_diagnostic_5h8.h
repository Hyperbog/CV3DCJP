/* 5H-8: read-only frame-boundary evidence, versioned binary sidecars. */
#ifndef CVIII_DIAGNOSTIC_5H8_H
#define CVIII_DIAGNOSTIC_5H8_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define CVIII_DIAG_LINES 240u
#define CVIII_DIAG_LINE_BYTES 5u
#define CVIII_DIAG_CHR_BYTES 8192u
#define CVIII_DIAG_RAM_BYTES 2048u
typedef struct {
    uint64_t frame, cycles;
    uint16_t pc;
    uint8_t a,x,y,s,p,nmi,irq;
    const uint8_t *ram; /* 2048 bytes, caller-owned */
    const uint8_t *oam; /* 256 bytes, caller-owned */
    const uint8_t *palette; /* 32 bytes */
    const uint8_t *ciram; size_t ciram_len;
    const uint8_t *lines; /* 240 x 5 byte serialized CycLine */
    const uint8_t *chr; /* current mapped PPU CHR, 8192 bytes */
} CviiiDiagnosticFrame5H8;
typedef struct {
    FILE *frames,*oam,*lines,*palette,*ram,*ciram,*chr,*events;
    unsigned full,seen,flush_count;
    uint8_t stage,area,room;
    uint32_t prior_chr,prior_oam,prior_lines;
    uint64_t last_frame;
} CviiiDiagnostic5H8;
/* `prefix` must refer to an owner-local path; appends explicit suffixes. */
int cviii_diag_open_5h8(CviiiDiagnostic5H8 *d,const char *prefix,int full);
int cviii_diag_append_5h8(CviiiDiagnostic5H8 *d,const CviiiDiagnosticFrame5H8 *f);
void cviii_diag_close_5h8(CviiiDiagnostic5H8 *d);
/* Host bridge: uses the public NESRecomp frame-boundary APIs. */
int cviii_diag_runtime_open_5h8(const char *prefix);
int cviii_diag_runtime_mode_5h8(const char *mode);
void cviii_diag_runtime_frame_5h8(uint64_t frame);
#endif
