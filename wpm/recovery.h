/** @file recovery.h @brief Read-only failure audit inspection. */
#ifndef WPM_RECOVERY_H
#define WPM_RECOVERY_H

/** List recorded failed upgrades without scripts, web retrieval, or writes.
 * Returns nonzero for a complete query, including an empty audit directory. */
int wpm_recovery_list(void);

/** Examine between 1 and 4096 matching entries; defaults remain unchanged. */
int wpm_recovery_list_limit(unsigned limit);

#endif
