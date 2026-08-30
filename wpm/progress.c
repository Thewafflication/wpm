/** @file progress.c @brief Shared byte-progress presentation. */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <windows.h>

#include "progress.h"

#define WPM_PROGRESS_INTERACTIVE_INTERVAL_MS 100UL
#define WPM_PROGRESS_SCRIPT_INTERVAL_MS 2000UL
#define WPM_PROGRESS_DEFAULT_CONSOLE_WIDTH 80
#define WPM_PROGRESS_BAR_MIN_WIDTH 10
#define WPM_PROGRESS_BAR_MAX_WIDTH 160

static int progress_stdout_is_interactive(void) {
    DWORD mode;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    return output != NULL && output != INVALID_HANDLE_VALUE &&
        GetConsoleMode(output, &mode);
}

static int progress_console_width(void) {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO information;
    int width;
    if (output == NULL || output == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(output, &information)) {
        return WPM_PROGRESS_DEFAULT_CONSOLE_WIDTH;
    }
    width = (int)information.srWindow.Right - (int)information.srWindow.Left + 1;
    if (width <= 0) width = (int)information.dwSize.X;
    return width > 0 ? width : WPM_PROGRESS_DEFAULT_CONSOLE_WIDTH;
}

static int progress_bar_width(int console_width, size_t fixed_width) {
    size_t usable;
    if (console_width <= 1 || (size_t)(console_width - 1) <= fixed_width) return 0;
    usable = (size_t)(console_width - 1) - fixed_width;
    if (usable < WPM_PROGRESS_BAR_MIN_WIDTH) return 0;
    if (usable > WPM_PROGRESS_BAR_MAX_WIDTH) usable = WPM_PROGRESS_BAR_MAX_WIDTH;
    return (int)usable;
}

static int progress_clear_console_line(void) {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO information;
    COORD beginning;
    DWORD written;
    if (output == NULL || output == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(output, &information)) return 0;
    beginning.X = 0;
    beginning.Y = information.dwCursorPosition.Y;
    return FillConsoleOutputCharacterA(output, ' ', (DWORD)information.dwSize.X,
        beginning, &written) && SetConsoleCursorPosition(output, beginning);
}

static void progress_write_line(wpm_progress* progress, const char* line, int final) {
    size_t length = strlen(line);
    if (!progress->interactive) {
        printf("%s\n", line);
        fflush(stdout);
        return;
    }
    if (progress_clear_console_line()) {
        printf("%s", line);
        progress->rendered_length = length;
    }
    else {
        printf("\r%s", line);
        if (progress->rendered_length > length) {
            size_t remaining = progress->rendered_length - length;
            while (remaining--) putchar(' ');
        }
        else {
            progress->rendered_length = length;
        }
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
        char bar[WPM_PROGRESS_BAR_MAX_WIDTH + 1];
        char prefix[384];
        char suffix[128];
        unsigned percent = 0;
        int bar_width;
        int console_width = progress_console_width();
        int compact_width;
        int prefix_length;
        int suffix_length;
        int i;
        if (progress->total) {
            int complete;
            percent = (unsigned)((progress->current * 100ULL) / progress->total);
            if (percent > 100) percent = 100;
            prefix_length = snprintf(prefix, sizeof(prefix), "%s %s [",
                progress->ongoing_verb, progress->label);
            suffix_length = snprintf(suffix, sizeof(suffix), "] %3u%% %llu/%llu bytes",
                percent, progress->current, progress->total);
            bar_width = prefix_length < 0 || suffix_length < 0 ? 0 :
                progress_bar_width(console_width,
                    (size_t)prefix_length + (size_t)suffix_length);
            if (bar_width > 0) {
                memset(bar, ' ', (size_t)bar_width);
                bar[bar_width] = '\0';
                complete = (int)((percent * (unsigned)bar_width) / 100);
                for (i = 0; i < complete; i++) bar[i] = '=';
                if (complete < bar_width) bar[complete] = '>';
                snprintf(line, sizeof(line), "%s%s%s", prefix, bar, suffix);
            }
            else {
                compact_width = snprintf(prefix, sizeof(prefix),
                    "%s : %3u%% %llu/%llu bytes", progress->ongoing_verb, percent,
                    progress->current, progress->total);
                compact_width = console_width > 1 && compact_width >= 0 &&
                    compact_width < console_width - 1 ?
                    console_width - 1 - compact_width : 0;
                snprintf(line, sizeof(line), "%s %.*s: %3u%% %llu/%llu bytes",
                    progress->ongoing_verb, compact_width, progress->label, percent,
                    progress->current, progress->total);
            }
        }
        else {
            int marker;
            prefix_length = snprintf(prefix, sizeof(prefix), "%s %s [",
                progress->ongoing_verb, progress->label);
            suffix_length = snprintf(suffix, sizeof(suffix), "]  --%% %llu bytes",
                progress->current);
            bar_width = prefix_length < 0 || suffix_length < 0 ? 0 :
                progress_bar_width(console_width,
                    (size_t)prefix_length + (size_t)suffix_length);
            if (bar_width > 0) {
                memset(bar, ' ', (size_t)bar_width);
                bar[bar_width] = '\0';
                marker = (int)((now / WPM_PROGRESS_INTERACTIVE_INTERVAL_MS) %
                    (DWORD)bar_width);
                bar[marker] = '>';
                snprintf(line, sizeof(line), "%s%s%s", prefix, bar, suffix);
            }
            else {
                compact_width = snprintf(prefix, sizeof(prefix), "%s : %llu bytes",
                    progress->ongoing_verb, progress->current);
                compact_width = console_width > 1 && compact_width >= 0 &&
                    compact_width < console_width - 1 ?
                    console_width - 1 - compact_width : 0;
                snprintf(line, sizeof(line), "%s %.*s: %llu bytes",
                    progress->ongoing_verb, compact_width, progress->label,
                    progress->current);
            }
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
