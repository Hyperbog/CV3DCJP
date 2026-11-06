/*
 * Collision v11 HEADLESS only: optional historical 5H8S/5H8U selector
 * write probes require private engine patches that are not present in pinned
 * upstream NESRecomp 2b4fcdb98c9e702be3fb365cfc645003c50b5fc1.
 *
 * This unit supplies only the HOST power-on function symbols, not fake
 * cyc_cviii_selector_write_* callbacks. No selector evidence is produced.
 * Original CPU/mapper/PPU/APU and cycle/DMA observation are untouched.
 * Linked only when JP_COLLISION_V11_SHADOW and JP_CYCLE_HEADLESS are ON.
 */
#include <stdio.h>
#include <stdlib.h>

#if !defined(CVIII_COLLISION_V11_SHADOW)
#error "This compatibility unit is only for opt-in collision v11 observers"
#endif

static void prohibit_selector_probe(const char *name)
{
    const char *value = getenv(name);
    if (value && *value) {
        fprintf(stderr, "collision v11: %s requires separate 5H8S/5H8U "
                "engine patch; this pinned observer intentionally has "
                "NO selector-write callback\n", name);
        fflush(stderr);
        abort();
    }
}

void cviii_selector_trace_power_on_5h8s(void)
{
    prohibit_selector_probe("CVIII_SELECTOR_TRACE_5H8S");
}

void cviii_selector_trace_power_on_5h8u(void)
{
    prohibit_selector_probe("CVIII_SELECTOR_TRACE_5H8U");
}
