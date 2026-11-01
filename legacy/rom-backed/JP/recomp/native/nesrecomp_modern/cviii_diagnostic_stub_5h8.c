/* Host-contract regression has no running NES machine; diagnostics are off. */
#include "cviii_diagnostic_5h8.h"
int cviii_diag_runtime_open_5h8(const char *p) { return p && *p; }
int cviii_diag_runtime_mode_5h8(const char *p) { return p && *p; }
void cviii_diag_runtime_frame_5h8(uint64_t frame) { (void)frame; }
