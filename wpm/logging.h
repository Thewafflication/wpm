/** @file logging.h @brief Operational logging and console adapters. */
#ifndef WPM_LOGGING_H
#define WPM_LOGGING_H

#include <stdarg.h>

#ifndef va_copy
/** Copy a variable-argument cursor on CRTs that omit C99 va_copy. */
#define va_copy(destination, source) ((destination) = (source))
#endif

/**
 * Open the configured WSP operational log in append mode.
 * @return Nonzero on success; zero if the log directory or file cannot open.
 */
int wpm_log_initialize(void);

/** Close the operational log if it is open. */
void wpm_log_close(void);

/**
 * Write formatted output to the console and active WSP log.
 * @param[in] format printf-compatible format string.
 * @return The console character count, or a negative output error.
 */
int wpm_printf(const char* format, ...);

/**
 * Write variable-argument output to the console and active WSP log.
 * @param[in] format printf-compatible format string.
 * @param[in] arguments Arguments consumed according to @p format.
 * @return The console character count, or a negative output error.
 */
int wpm_vprintf(const char* format, va_list arguments);

#ifndef WPM_LOGGING_IMPLEMENTATION
/** Route project-owned printf calls through the WSP logging adapter. */
#define printf wpm_printf
/** Route project-owned vprintf calls through the WSP logging adapter. */
#define vprintf wpm_vprintf
#endif

#endif
