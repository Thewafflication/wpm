#include "logging.h"
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

/* Progress must not depend on CRT stdout buffering while moving the native
   console cursor. WriteFile works on XP consoles and redirected handles. */
static int compact_progress;
static int live_line;
static int parallel_progress;
static wpm_progress *live_downloads[8];
static int live_count, rendered_rows;
static COORD live_origin;
void wpm_progress_parallel(int enabled) { parallel_progress = enabled; }
void wpm_progress_compact(int enabled) { compact_progress = enabled; }

static void progress_write(const char *text) {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    size_t remaining = strlen(text);
    fflush(stdout);
    while (remaining) {
        DWORD written;
        DWORD requested = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
        if (!WriteFile(output, text, requested, &written, NULL) || !written) {
            return;
        }
        text += written;
        remaining -= written;
    }
}

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
    width =
        (int)information.srWindow.Right - (int)information.srWindow.Left + 1;
    if (width <= 0) {
        width = (int)information.dwSize.X;
    }
    return width > 0 ? width : WPM_PROGRESS_DEFAULT_CONSOLE_WIDTH;
}

static int progress_bar_width(int console_width, size_t fixed_width) {
    size_t usable;
    if (console_width <= 1 || (size_t)(console_width - 1) <= fixed_width) {
        return 0;
    }
    usable = (size_t)(console_width - 1) - fixed_width;
    if (usable < WPM_PROGRESS_BAR_MIN_WIDTH) {
        return 0;
    }
    if (usable > WPM_PROGRESS_BAR_MAX_WIDTH) {
        usable = WPM_PROGRESS_BAR_MAX_WIDTH;
    }
    return (int)usable;
}

static int progress_clear_console_line(void) {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO information;
    COORD beginning;
    DWORD written;
    if (output == NULL || output == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(output, &information)) {
        return 0;
    }
    beginning.X = 0;
    beginning.Y = information.dwCursorPosition.Y;
    return FillConsoleOutputCharacterA(output, ' ', (DWORD)information.dwSize.X,
                                       beginning, &written) &&
           SetConsoleCursorPosition(output, beginning);
}

void wpm_progress_clear_display(void) {
    if (rendered_rows) {
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO information;
        DWORD written;
        if (GetConsoleScreenBufferInfo(output, &information)) {
            for (int i = 0; i < rendered_rows; i++) {
                COORD row = live_origin;
                row.Y += (SHORT)i;
                FillConsoleOutputCharacterA(output, ' ', information.dwSize.X,
                                            row, &written);
            }
            SetConsoleCursorPosition(output, live_origin);
        }
        rendered_rows = 0;
    }
    if (live_line) {
        progress_clear_console_line();
        live_line = 0;
    }
}

void wpm_progress_restore_display(void) {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO information;
    if (!live_count) {
        return;
    }
    for (int i = 0; i < live_count; i++) {
        char line[512];
        int width = progress_console_width();
        snprintf(line, sizeof(line), "%.*s", width > 1 ? width - 1 : 0,
                 live_downloads[i]->live_text);
        progress_write(line);
        progress_write("\r\n");
    }
    if (GetConsoleScreenBufferInfo(output, &information)) {
        live_origin.X = 0;
        live_origin.Y = information.dwCursorPosition.Y - (SHORT)live_count;
        rendered_rows = live_count;
    }
}

static void progress_write_line(wpm_progress *progress, const char *line,
                                int final) {
    size_t length = strlen(line);
    wpm_output_lock();
    if (progress->dashboard) {
        int slot = 0;
        wpm_progress_clear_display();
        while (slot < live_count && live_downloads[slot] != progress) {
            slot++;
        }
        if (final) {
            if (slot < live_count) {
                for (int i = slot; i + 1 < live_count; i++) {
                    live_downloads[i] = live_downloads[i + 1];
                }
                live_count--;
            }
            progress_write(line);
            progress_write("\r\n");
        } else {
            snprintf(progress->live_text, sizeof(progress->live_text), "%s",
                     line);
            if (slot == live_count && live_count < 8) {
                live_downloads[live_count++] = progress;
            }
        }
        wpm_progress_restore_display();
        wpm_output_unlock();
        return;
    }
    if (!progress->interactive) {
        progress_write(line);
        progress_write("\r\n");
        wpm_output_unlock();
        return;
    }
    if (progress_clear_console_line()) {
        progress_write(line);
        progress->rendered_length = length;
    } else {
        progress_write("\r");
        progress_write(line);
        if (progress->rendered_length > length) {
            size_t remaining = progress->rendered_length - length;
            while (remaining--) {
                progress_write(" ");
            }
        } else {
            progress->rendered_length = length;
        }
    }
    live_line = !final;
    if (final) {
        progress_write("\r\n");
    }
    wpm_output_unlock();
}

static void progress_render(wpm_progress *progress, int force) {
    DWORD now = GetTickCount();
    DWORD interval = progress->interactive
                         ? WPM_PROGRESS_INTERACTIVE_INTERVAL_MS
                         : WPM_PROGRESS_SCRIPT_INTERVAL_MS;
    char line[512];

    if (!progress->started || (progress->compact && !progress->interactive)) {
        return;
    }
    if (!force && (DWORD)(now - progress->last_update_ms) < interval) {
        return;
    }
    progress->last_update_ms = now;

    if (progress->interactive) {
        char bar[WPM_PROGRESS_BAR_MAX_WIDTH + 1];
        char label[384];
        char prefix[384];
        char suffix[128];
        unsigned percent = 0;
        int bar_width;
        int console_width = progress_console_width();
        int compact_width;
        int prefix_length;
        int suffix_length;
        int i;
        int label_width = console_width - 60;
        if (label_width < 8) {
            label_width = 8;
        }
        if ((size_t)label_width >= sizeof(label)) {
            label_width = (int)sizeof(label) - 1;
        }
        const char *display_label = progress->label;
        if (!strncmp(display_label, "index ", 6) &&
            !strstr(display_label, "://") && strchr(display_label, '@')) {
            const char *repository = strchr(display_label, '/');
            if (repository) {
                display_label = repository + 1;
            }
        }
        snprintf(label, sizeof(label), "%.*s", label_width, display_label);
        if (strlen(display_label) > (size_t)label_width && label_width > 3) {
            memcpy(label + label_width - 3, "...", 3);
        }
        if (progress->total) {
            int complete;
            percent =
                (unsigned)((progress->current * 100ULL) / progress->total);
            if (percent > 100) {
                percent = 100;
            }
            prefix_length = snprintf(prefix, sizeof(prefix), "%s %s [",
                                     progress->ongoing_verb, label);
            suffix_length =
                snprintf(suffix, sizeof(suffix), "] %3u%% %llu/%llu bytes",
                         percent, progress->current, progress->total);
            bar_width = prefix_length < 0 || suffix_length < 0
                            ? 0
                            : progress_bar_width(console_width,
                                                 (size_t)prefix_length +
                                                     (size_t)suffix_length);
            if (bar_width > 0) {
                memset(bar, ' ', (size_t)bar_width);
                bar[bar_width] = '\0';
                complete = (int)((percent * (unsigned)bar_width) / 100);
                for (i = 0; i < complete; i++) {
                    bar[i] = '=';
                }
                if (complete < bar_width) {
                    bar[complete] = '>';
                }
                snprintf(line, sizeof(line), "%s%s%s", prefix, bar, suffix);
            } else {
                compact_width = snprintf(prefix, sizeof(prefix),
                                         "%s : %3u%% %llu/%llu bytes",
                                         progress->ongoing_verb, percent,
                                         progress->current, progress->total);
                compact_width = console_width > 1 && compact_width >= 0 &&
                                        compact_width < console_width - 1
                                    ? console_width - 1 - compact_width
                                    : 0;
                snprintf(line, sizeof(line), "%s %.*s: %3u%% %llu/%llu bytes",
                         progress->ongoing_verb, compact_width, progress->label,
                         percent, progress->current, progress->total);
            }
        } else {
            int marker;
            prefix_length = snprintf(prefix, sizeof(prefix), "%s %s [",
                                     progress->ongoing_verb, label);
            suffix_length = snprintf(suffix, sizeof(suffix),
                                     "]  --%% %llu bytes", progress->current);
            bar_width = prefix_length < 0 || suffix_length < 0
                            ? 0
                            : progress_bar_width(console_width,
                                                 (size_t)prefix_length +
                                                     (size_t)suffix_length);
            if (bar_width > 0) {
                memset(bar, ' ', (size_t)bar_width);
                bar[bar_width] = '\0';
                marker = (int)((now / WPM_PROGRESS_INTERACTIVE_INTERVAL_MS) %
                               (DWORD)bar_width);
                bar[marker] = '>';
                snprintf(line, sizeof(line), "%s%s%s", prefix, bar, suffix);
            } else {
                compact_width =
                    snprintf(prefix, sizeof(prefix), "%s : %llu bytes",
                             progress->ongoing_verb, progress->current);
                compact_width = console_width > 1 && compact_width >= 0 &&
                                        compact_width < console_width - 1
                                    ? console_width - 1 - compact_width
                                    : 0;
                snprintf(line, sizeof(line), "%s %.*s: %llu bytes",
                         progress->ongoing_verb, compact_width, progress->label,
                         progress->current);
            }
        }
    } else if (progress->total) {
        unsigned percent =
            (unsigned)((progress->current * 100ULL) / progress->total);
        if (percent > 100) {
            percent = 100;
        }
        snprintf(line, sizeof(line), "%s progress: %s: %u%% (%llu/%llu bytes)",
                 progress->noun, progress->label, percent, progress->current,
                 progress->total);
    } else {
        snprintf(line, sizeof(line), "%s progress: %s: %llu bytes",
                 progress->noun, progress->label, progress->current);
    }
    progress_write_line(progress, line, 0);
}

void wpm_progress_start(wpm_progress *progress, const char *ongoing_verb,
                        const char *noun, const char *completed_verb,
                        const char *label, unsigned long long total) {
    memset(progress, 0, sizeof(*progress));
    if (!label) {
        return;
    }
    progress->ongoing_verb = ongoing_verb;
    progress->noun = noun;
    progress->completed_verb = completed_verb;
    progress->label = label;
    progress->total = total;
    progress->last_update_ms = GetTickCount();
    progress->compact = compact_progress;
    progress->interactive = progress_stdout_is_interactive();
    progress->dashboard = parallel_progress && progress->interactive;
    progress->started = 1;
    progress_render(progress, 1);
}

void wpm_progress_set(wpm_progress *progress, unsigned long long current,
                      unsigned long long total) {
    int total_changed;
    if (!progress || !progress->started) {
        return;
    }
    total_changed = progress->total != total;
    progress->current = current;
    progress->total = total;
    /* Show newly available size information even for short index downloads. */
    progress_render(progress, total_changed);
}

void wpm_progress_add(wpm_progress *progress, unsigned long long bytes) {
    if (!progress || !progress->started) {
        return;
    }
    if (ULLONG_MAX - progress->current < bytes) {
        progress->current = ULLONG_MAX;
    } else {
        progress->current += bytes;
    }
    progress_render(progress, 0);
}

void wpm_progress_finish(wpm_progress *progress, int succeeded) {
    char line[512];
    int label_width = 300;
    if (!progress || !progress->started) {
        return;
    }
    if (succeeded && progress->total) {
        progress->current = progress->total;
    }
    if (progress->compact) {
        SYSTEMTIME now;
        GetLocalTime(&now);
        snprintf(line, sizeof(line), "[%02u:%02u:%02u] %s %.*s (%llu bytes)",
                 now.wHour, now.wMinute, now.wSecond,
                 succeeded ? progress->completed_verb : "Failed", label_width,
                 progress->label, progress->current);
        progress_write_line(progress, line, 1);
        progress->started = 0;
        return;
    }
    if (progress->interactive) {
        int fixed = snprintf(
            line, sizeof(line),
            succeeded ? "%s : %llu bytes" : "%s failed:  after %llu bytes",
            succeeded ? progress->completed_verb : progress->noun,
            progress->current);
        int available = progress_console_width() - fixed - 1;
        if (available < label_width) {
            label_width = available;
        }
        if (label_width < 0) {
            label_width = 0;
        }
    }
    if (succeeded) {
        snprintf(line, sizeof(line), "%s %.*s: %llu bytes",
                 progress->completed_verb, label_width, progress->label,
                 progress->current);
    } else {
        snprintf(line, sizeof(line), "%s failed: %.*s after %llu bytes",
                 progress->noun, label_width, progress->label,
                 progress->current);
    }
    progress_write_line(progress, line, 1);
    progress->started = 0;
}
