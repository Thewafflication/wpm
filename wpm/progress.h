/** @file progress.h @brief Shared byte-progress presentation. */
#ifndef WPM_PROGRESS_H
#define WPM_PROGRESS_H

#include <stddef.h>

typedef struct wpm_progress {
    const char *ongoing_verb;
    const char *noun;
    const char *completed_verb;
    const char *label;
    unsigned long last_update_ms;
    unsigned long long current;
    unsigned long long total;
    size_t rendered_length;
    char live_text[512];
    int dashboard;
    int compact;
    int interactive;
    int started;
} wpm_progress;

/** Start byte progress; a NULL label disables presentation. */
void wpm_progress_start(wpm_progress *progress, const char *ongoing_verb,
                        const char *noun, const char *completed_verb,
                        const char *label, unsigned long long total);

/** Replace the current and total byte counts and render when due. */
void wpm_progress_set(wpm_progress *progress, unsigned long long current,
                      unsigned long long total);

/** Add completed bytes and render when due. */
void wpm_progress_add(wpm_progress *progress, unsigned long long bytes);

/** Finish the operation with a complete or failed presentation. */
void wpm_progress_finish(wpm_progress *progress, int succeeded);

/** Keep terminal bars but use completion-only records in redirected output. */
void wpm_progress_compact(int enabled);

/** Give concurrent terminal transfers separate live rows. */
void wpm_progress_parallel(int enabled);

/** Clear a live bar before another console message (caller holds output lock).
 */
void wpm_progress_clear_display(void);
void wpm_progress_restore_display(void);

#endif
