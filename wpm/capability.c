/** @file capability.c @brief Enforce process-local execution capabilities. */
#include "capability.h"
#include "logging.h"
static unsigned int capabilities =
    WPM_CAP_WRITE | WPM_CAP_SCRIPT | WPM_CAP_HANDOFF | WPM_CAP_NETWORK;
void wpm_set_readonly(int enabled) {
    if (enabled) {
        capabilities = 0;
    }
}
int wpm_is_readonly(void) { return capabilities == 0; }
int wpm_require_capability(unsigned int required, const char *operation) {
    if ((capabilities & required) == required) {
        return 1;
    }
    wpm_event_printf(WPM_EVENT_ERROR, "capability.denied", "plan", "authorize",
                     "Error: dry-run cannot authorize %s.\n", operation);
    return 0;
}
