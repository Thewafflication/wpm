/** @file installed.h @brief Durable installation metadata, separate from
 * archives. */
#ifndef WPM_INSTALLED_H
#define WPM_INSTALLED_H
#include "archive.h"
/** Read durable records plus legacy archives without writing. Invalid records
 * count as issues; callers must not treat a partial scan as complete. */
int wpm_installed_read(wpm_package_info *items, int capacity, int *count,
                       int *issues, int *limited);
/** Persist metadata only after successful deployment, using atomic publication.
 */
int wpm_installed_save(const wpm_package_info *info);
/** Migrate readable retained legacy archives, without executing their contents.
 */
int wpm_installed_migrate(void);
/** Remove metadata after successful package removal. Missing is idempotent. */
int wpm_installed_forget(const char *archive_name);
#endif
