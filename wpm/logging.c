/** @file logging.c @brief WSP-backed operational logging integration. */
#define WPM_LOGGING_IMPLEMENTATION
#include "logging.h"

#include "archive.h"
#include "helpers.h"
#ifndef WPM_DISABLE_OPERATIONAL_LOG
#include "wsp_log.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifndef WPM_DISABLE_OPERATIONAL_LOG
static wsp_logger wpm_logger;
static int wpm_logger_initialized;

static wsp_log_level wpm_log_level_for_message(const char* message)
{
    if (strncmp(message, "Error:", 6) == 0) return WSP_LOG_ERROR;
    if (strncmp(message, "Warning:", 8) == 0) return WSP_LOG_WARN;
    if (strncmp(message, "Verbose:", 8) == 0) return WSP_LOG_DEBUG;
    if (strncmp(message, "Result:", 7) == 0 ||
        strncmp(message, "Installed ", 10) == 0 ||
        strncmp(message, "Built package:", 14) == 0 ||
        strncmp(message, "Verified package:", 17) == 0 ||
        strncmp(message, "Removed package:", 16) == 0 ||
        strncmp(message, "Upgraded ", 9) == 0) return WSP_LOG_PASS;
    return WSP_LOG_INFO;
}
#endif

int wpm_log_initialize(void)
{
#ifdef WPM_DISABLE_OPERATIONAL_LOG
    return 1;
#else
    char data_root[4096];
    char log_directory[4096];
    char log_path[4096];
    const char* configured_path;

    if (wpm_logger_initialized) return 1;
    wsp_log_init(&wpm_logger);
    wsp_log_set_console_level(&wpm_logger, WSP_LOG_OFF);
    configured_path = getenv("WPM_LOG_FILE");
    if (configured_path && configured_path[0]) {
        if (wsp_log_open_file(&wpm_logger, configured_path, 1) != 0) return 0;
    }
    else {
        if (!wpm_get_data_root(data_root, sizeof(data_root)) ||
            snprintf(log_directory, sizeof(log_directory), "%s\\audit", data_root) <= 0 ||
            (!CreateDirectoryA(log_directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) ||
            snprintf(log_path, sizeof(log_path), "%s\\audit\\wpm.log", data_root) <= 0 ||
            wsp_log_open_file(&wpm_logger, log_path, 1) != 0) return 0;
    }
    wpm_logger_initialized = 1;
    return 1;
#endif
}

void wpm_log_close(void)
{
#ifndef WPM_DISABLE_OPERATIONAL_LOG
    if (wpm_logger_initialized) wsp_log_close(&wpm_logger);
    wpm_logger_initialized = 0;
#endif
}

int wpm_vprintf(const char* format, va_list arguments)
{
#ifdef WPM_DISABLE_OPERATIONAL_LOG
    /* Some supported CRTs do not provide a deep-copyable va_list. Replaying
       one argument list for both console and file formatting corrupts the
       cursor and can crash after otherwise successful commands. */
    return vprintf(format, arguments);
#else
    char message[8192];
    size_t length;
    int result;
    va_list console_arguments;

    va_copy(console_arguments, arguments);
    result = vprintf(format, console_arguments);
    va_end(console_arguments);
    if (wpm_logger_initialized && vsnprintf(message, sizeof(message), format, arguments) >= 0) {
        length = strlen(message);
        while (length > 0 && (message[length - 1] == '\n' || message[length - 1] == '\r')) {
            message[--length] = '\0';
        }
        if (length > 0) {
            wsp_log_write(&wpm_logger, wpm_log_level_for_message(message), "%s", message);
        }
    }
    return result;
#endif
}

int wpm_printf(const char* format, ...)
{
    int result;
    va_list arguments;

    va_start(arguments, format);
    result = wpm_vprintf(format, arguments);
    va_end(arguments);
    return result;
}
