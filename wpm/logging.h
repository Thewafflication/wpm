/** @file logging.h @brief Operational logging and console adapters. */
#ifndef WPM_LOGGING_H
#define WPM_LOGGING_H

#include <stdarg.h>

/** Semantic command event classes, independent of human wording. */
typedef enum wpm_event_class {
    WPM_EVENT_INFO,
    WPM_EVENT_PROGRESS,
    WPM_EVENT_SUCCESS,
    WPM_EVENT_WARNING,
    WPM_EVENT_ERROR,
    WPM_EVENT_PROMPT,
    WPM_EVENT_SCRIPT,
    WPM_EVENT_RESULT,
    WPM_EVENT_DEBUG
} wpm_event_class;

/** Emit a typed event after shared redaction and terminal-byte filtering. */
int wpm_event_printf(wpm_event_class kind, const char *code,
                     const char *operation, const char *phase,
                     const char *format, ...);

/** Install an optional process-local renderer for already-redacted events.
 * A sink replaces console rendering; it must not retain the borrowed text.
 */
typedef void (*wpm_event_sink)(wpm_event_class kind, const char *code,
                               const char *operation, const char *phase,
                               const char *text);
void wpm_event_set_sink(wpm_event_sink sink);

/** Apply the shared terminal/credential filter to mutable NUL-terminated text.
 * Filtering only shortens text; callers retain ownership of the buffer. */
void wpm_redact_text(char *text);

/**
 * Open the configured WSP operational log in append mode.
 * @return Nonzero on success; zero if the log directory or file cannot open.
 */
int wpm_log_initialize(void);

/** Configure human-output styling. Accepted values are auto, always, never. */
int wpm_set_color_policy(const char *value);

/** Close the operational log if it is open. */
void wpm_log_close(void);

/**
 * Write formatted output to the console and active WSP log.
 * @param[in] format printf-compatible format string.
 * @return The console character count, or a negative output error.
 */
int wpm_printf(const char *format, ...);

/**
 * Write variable-argument output to the console and active WSP log.
 * @param[in] format printf-compatible format string.
 * @param[in] arguments Arguments consumed according to @p format.
 * @return The console character count, or a negative output error.
 */
int wpm_vprintf(const char *format, va_list arguments);

#ifndef WPM_LOGGING_IMPLEMENTATION
/** Route project-owned printf calls through the WSP logging adapter. */
#define printf wpm_printf
/** Route project-owned vprintf calls through the WSP logging adapter. */
#define vprintf wpm_vprintf
#endif

#endif
