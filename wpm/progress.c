/** @file progress.c @brief Shared byte-progress presentation. */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <windows.h>

#include "progress.h"

#define WPM_PROGRESS_INTERACTIVE_INTERVAL_MS 100UL
#define WPM_PROGRESS_SCRIPT_INTERVAL_MS 2000UL
#define WPM_PROGRESS_BAR_WIDTH 30

static int progress_stdout_is_interactive(void) {
    DWORD mode;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    return output != NULL && output != INVALID_HANDLE_VALUE &&
        GetConsoleMode(output, &mode);
}

static void progress_write_line(wpm_progress* progress, const char* line, int final) {
    size_t length = strlen(line);
    if (!progress->interactive) {
        printf("%s\n", line);
        fflush(stdout);
        return;
    }
    printf("\r%s", line);
    if (progress->rendered_length > length) {
        size_t remaining = progress->rendered_length - length;
        while (remaining--) putchar(' ');
    }
    else {
        progress->rendered_length = length;
    }
    if (final) putchar('\n');
    fflush(stdout);
}

static void progress_render(wpm_progress* progress, int force) {
    DWORD now = GetTickCount();
    DWORD interval = progress->interactive ?
        WPM_PROGRESS_INTERACTIVE_INTERVAL_MS : WPM_PROGRESS_SCRIPT_INTERVAL_MS;
    char line[512];

    if (!progress->started) return;
    if (!force && (DWORD)(now - progress->last_update_ms) < interval) return;
    progress->last_update_ms = now;

    if (progress->interactive) {
        char bar[WPM_PROGRESS_BAR_WIDTH + 1];
        unsigned percent = 0;
        int i;
        memset(bar, ' ', WPM_PROGRESS_BAR_WIDTH);
        bar[WPM_PROGRESS_BAR_WIDTH] = '\0';
        if (progress->total) {
            int complete;
            percent = (unsigned)((progress->current * 100ULL) / progress->total);
            if (percent > 100) percent = 100;
            complete = (int)((percent * WPM_PROGRESS_BAR_WIDTH) / 100);
            for (i = 0; i < complete; i++) bar[i] = '=';
            if (complete < WPM_PROGRESS_BAR_WIDTH) bar[complete] = '>';
            snprintf(line, sizeof(line), "%s %s [%s] %3u%% %llu/%llu bytes",
                progress->ongoing_verb, progress->label, bar, percent,
                progress->current, progress->total);
        }
        else {
            int marker = (int)((now / WPM_PROGRESS_INTERACTIVE_INTERVAL_MS) %
                WPM_PROGRESS_BAR_WIDTH);
            bar[marker] = '>';
            snprintf(line, sizeof(line), "%s %s [%s]  --%% %llu bytes",
                progress->ongoing_verb, progress->label, bar, progress->current);
        }
    }
    else if (progress->total) {
        unsigned percent = (unsigned)((progress->current * 100ULL) / progress->total);
        if (percent > 100) percent = 100;
        snprintf(line, sizeof(line), "%s progress: %s: %u%% (%llu/%llu bytes)",
            progress->noun, progress->label, percent,
            progress->current, progress->total);
    }
    else {
        snprintf(line, sizeof(line), "%s progress: %s: %llu bytes",
            progress->noun, progress->label, progress->current);
    }
    progress_write_line(progress, line, 0);
}

void wpm_progress_start(
    wpm_progress* progress,
    const char* ongoing_verb,
    const char* noun,
    const char* completed_verb,
    const char* label,
    unsigned long long total
) {
    memset(progress, 0, sizeof(*progress));
    progress->ongoing_verb = ongoing_verb;
    progress->noun = noun;
    progress->completed_verb = completed_verb;
    progress->label = label;
    progress->total = total;
    progress->last_update_ms = GetTickCount();
    progress->interactive = progress_stdout_is_interactive();
    progress->started = 1;
    progress_render(progress, 1);
}

void wpm_progress_set(
    wpm_progress* progress,
    unsigned long long current,
    unsigned long long total
) {
    if (!progress || !progress->started) return;
    progress->current = current;
    progress->total = total;
    progress_render(progress, 0);
}

void wpm_progress_add(wpm_progress* progress, unsigned long long bytes) {
    if (!progress || !progress->started) return;
    if (ULLONG_MAX - progress->current < bytes) progress->current = ULLONG_MAX;
    else progress->current += bytes;
    progress_render(progress, 0);
}

void wpm_progress_finish(wpm_progress* progress, int succeeded) {
    char line[512];
    if (!progress || !progress->started) return;
    if (succeeded && progress->total) progress->current = progress->total;
    if (progress->interactive && succeeded && progress->total) {
        progress_render(progress, 1);
        putchar('\n');
        fflush(stdout);
    }
    else {
        if (succeeded) {
            snprintf(line, sizeof(line), "%s %s: %llu bytes",
                progress->completed_verb, progress->label, progress->current);
        }
        else {
            snprintf(line, sizeof(line), "%s failed: %s after %llu bytes",
                progress->noun, progress->label, progress->current);
        }
        progress_write_line(progress, line, 1);
    }
    progress->started = 0;
}
