/** @file capability.h @brief Process-local mutation and execution authority. */
#ifndef WPM_CAPABILITY_H
#define WPM_CAPABILITY_H
typedef enum wpm_capability {
    WPM_CAP_WRITE = 1,
    WPM_CAP_SCRIPT = 2,
    WPM_CAP_HANDOFF = 4,
    WPM_CAP_NETWORK = 8
} wpm_capability;
/** Reduce this invocation to read-only authority. Environment cannot grant it.
 */
void wpm_set_readonly(int enabled);
/** Whether this invocation has read-only authority. */
int wpm_is_readonly(void);
/** Deny unavailable capabilities at the mutation/dispatch boundary. */
int wpm_require_capability(unsigned int required, const char *operation);
#endif
