/** @file progress.h @brief Shared byte-progress presentation. */
#ifndef WPM_PROGRESS_H
#define WPM_PROGRESS_H

#include <stddef.h>

typedef struct wpm_progress {
    const char* ongoing_verb;
    const char* noun;
    const char* completed_verb;
    const char* label;
    unsigned long last_update_ms;
    unsigned long long current;
    unsigned long long total;
    size_t rendered_length;
    int interactive;
    int started;
} wpm_progress;

/** Start and immediately render a byte-progress operation. */
void wpm_progress_start(
    wpm_progress* progress,
    const char* ongoing_verb,
    const char* noun,
    const char* completed_verb,
    const char* label,
    unsigned long long total
);

/** Replace the current and total byte counts and render when due. */
void wpm_progress_set(
    wpm_progress* progress,
    unsigned long long current,
    unsigned long long total
);

/** Add completed bytes and render when due. */
void wpm_progress_add(wpm_progress* progress, unsigned long long bytes);

/** Finish the operation with a complete or failed presentation. */
void wpm_progress_finish(wpm_progress* progress, int succeeded);

#endif
