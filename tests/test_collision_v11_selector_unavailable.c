/* ROM-free: must resolve legacy selector host entry points without providing
 * or fabricating absent upstream cyc_cviii_selector_write_* callbacks. */
#include <stdlib.h>
void cviii_selector_trace_power_on_5h8s(void);
void cviii_selector_trace_power_on_5h8u(void);
int main(void)
{
    cviii_selector_trace_power_on_5h8s();
    cviii_selector_trace_power_on_5h8u();
    return EXIT_SUCCESS;
}
