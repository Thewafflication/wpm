/** @file logging.c @brief WSP-backed operational logging integration. */
#define WPM_LOGGING_IMPLEMENTATION
#include "logging.h"

#include "archive.h"
#include "helpers.h"
#ifndef WPM_NATIVE_OPERATIONAL_LOG
#include "wsp_log.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef enum wpm_color_policy {
    WPM_COLOR_AUTO,
    WPM_COLOR_ALWAYS,
    WPM_COLOR_NEVER
} wpm_color_policy;

typedef enum wpm_message_style {
    WPM_STYLE_NONE,
    WPM_STYLE_PROGRESS,
    WPM_STYLE_SUCCESS,
    WPM_STYLE_WARNING,
    WPM_STYLE_ERROR,
    WPM_STYLE_PROMPT,
    WPM_STYLE_SCRIPT,
    WPM_STYLE_RESULT
} wpm_message_style;

static wpm_color_policy wpm_color = WPM_COLOR_AUTO;

int wpm_set_color_policy(const char* value)
{
    if (!value) return 0;
    if (strcmp(value, "auto") == 0) wpm_color = WPM_COLOR_AUTO;
    else if (strcmp(value, "always") == 0) wpm_color = WPM_COLOR_ALWAYS;
    else if (strcmp(value, "never") == 0) wpm_color = WPM_COLOR_NEVER;
    else return 0;
    return 1;
}

static int wpm_starts_with(const char* message, const char* prefix)
{
    return strncmp(message, prefix, strlen(prefix)) == 0;
}

static wpm_message_style wpm_message_style_for(const char* message)
{
    if (wpm_starts_with(message, "Error:")) return WPM_STYLE_ERROR;
    if (wpm_starts_with(message, "Warning:")) return WPM_STYLE_WARNING;
    if (wpm_starts_with(message, "Result:")) return WPM_STYLE_RESULT;
    if (wpm_starts_with(message, "Prompt:")) return WPM_STYLE_PROMPT;
    if (wpm_starts_with(message, "--- ")) return WPM_STYLE_SCRIPT;
    if (wpm_starts_with(message, "Installed ") ||
        wpm_starts_with(message, "Built package:") ||
        wpm_starts_with(message, "Verified package:") ||
        wpm_starts_with(message, "Removed package:") ||
        wpm_starts_with(message, "Upgraded ")) return WPM_STYLE_SUCCESS;
    if (strstr(message, " progress:") || wpm_starts_with(message, "Downloading ") ||
        wpm_starts_with(message, "Extracting ") ||
        wpm_starts_with(message, "Validating ")) return WPM_STYLE_PROGRESS;
    return WPM_STYLE_NONE;
}

static WORD wpm_console_attributes_for(wpm_message_style style)
{
    switch (style) {
        case WPM_STYLE_PROGRESS: return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case WPM_STYLE_SUCCESS: return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case WPM_STYLE_WARNING: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case WPM_STYLE_ERROR: return FOREGROUND_RED | FOREGROUND_INTENSITY;
        case WPM_STYLE_PROMPT: return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case WPM_STYLE_SCRIPT: return FOREGROUND_BLUE | FOREGROUND_GREEN;
        case WPM_STYLE_RESULT: return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        default: return 0;
    }
}

static const char* wpm_ansi_for(wpm_message_style style)
{
    switch (style) {
        case WPM_STYLE_PROGRESS: return "\x1b[96m";
        case WPM_STYLE_SUCCESS: return "\x1b[92m";
        case WPM_STYLE_WARNING: return "\x1b[93m";
        case WPM_STYLE_ERROR: return "\x1b[91m";
        case WPM_STYLE_PROMPT: return "\x1b[95m";
        case WPM_STYLE_SCRIPT: return "\x1b[36m";
        case WPM_STYLE_RESULT: return "\x1b[96m";
        default: return "";
    }
}

static int wpm_write_console_message(const char* message, wpm_message_style style)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO information;
    int interactive = output != NULL && output != INVALID_HANDLE_VALUE &&
        GetConsoleScreenBufferInfo(output, &information);

    if (style == WPM_STYLE_NONE || wpm_color == WPM_COLOR_NEVER ||
        (wpm_color == WPM_COLOR_AUTO && !interactive)) return fputs(message, stdout);
    if (interactive) {
        WORD attributes = wpm_console_attributes_for(style);
        SetConsoleTextAttribute(output, attributes);
        fputs(message, stdout);
        SetConsoleTextAttribute(output, information.wAttributes);
        return (int)strlen(message);
    }
    fputs(wpm_ansi_for(style), stdout);
    fputs(message, stdout);
    fputs("\x1b[0m", stdout);
    return (int)strlen(message);
}

#ifdef WPM_NATIVE_OPERATIONAL_LOG
static HANDLE wpm_log_handle = INVALID_HANDLE_VALUE;
static int wpm_logger_initialized;
static int wpm_native_file_level;

static const char* wpm_native_level_for_message(const char* message, int* level)
{
    if (strncmp(message, "Error:", 6) == 0) { *level = 4; return "ERROR"; }
    if (strncmp(message, "Warning:", 8) == 0) { *level = 3; return "WARN"; }
    if (strncmp(message, "Verbose:", 8) == 0) { *level = 0; return "DEBUG"; }
    if (strncmp(message, "Result:", 7) == 0 ||
        strncmp(message, "Installed ", 10) == 0 ||
        strncmp(message, "Built package:", 14) == 0 ||
        strncmp(message, "Verified package:", 17) == 0 ||
        strncmp(message, "Removed package:", 16) == 0 ||
        strncmp(message, "Upgraded ", 9) == 0) { *level = 2; return "PASS"; }
    *level = 1;
    return "INFO";
}

static void wpm_native_log_write(const char* message)
{
    const char* cursor = message;
    int level;
    const char* level_name = wpm_native_level_for_message(message, &level);
    if (!wpm_logger_initialized || level < wpm_native_file_level) return;
    while (*cursor) {
        SYSTEMTIME now;
        const char* end = cursor;
        char line[8448];
        int prefix_length;
        size_t content_length;
        DWORD written;
        while (*end && *end != '\r' && *end != '\n') end++;
        content_length = (size_t)(end - cursor);
        if (content_length > 0) {
            GetSystemTime(&now);
            prefix_length = snprintf(line, sizeof(line),
                "[%04u-%02u-%02uT%02u:%02u:%02u.%03uZ] [%s] ",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
                now.wSecond, now.wMilliseconds, level_name);
            if (prefix_length > 0 && (size_t)prefix_length + content_length + 2 < sizeof(line)) {
                memcpy(line + prefix_length, cursor, content_length);
                line[prefix_length + content_length] = '\r';
                line[prefix_length + content_length + 1] = '\n';
                WriteFile(wpm_log_handle, line,
                    (DWORD)((size_t)prefix_length + content_length + 2), &written, NULL);
            }
        }
        while (*end == '\r' || *end == '\n') end++;
        cursor = end;
    }
}
#else
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
    char data_root[4096];
    char log_directory[4096];
    char log_path[4096];
    char configured_path[4096];
    char configured_level[32];
#ifdef WPM_NATIVE_OPERATIONAL_LOG
    const char* selected_path;
#else
    wsp_log_level file_level = WSP_LOG_DEBUG;
#endif

    if (wpm_logger_initialized) return 1;
    if (wpm_get_environment_variable("WPM_LOG_LEVEL", configured_level,
            sizeof(configured_level))) {
#ifdef WPM_NATIVE_OPERATIONAL_LOG
        if (_stricmp(configured_level, "normal") == 0) wpm_native_file_level = 1;
#else
        if (_stricmp(configured_level, "normal") == 0) file_level = WSP_LOG_INFO;
#endif
        else if (_stricmp(configured_level, "verbose") != 0) return 0;
    }
#ifndef WPM_NATIVE_OPERATIONAL_LOG
    wsp_log_init(&wpm_logger);
    wsp_log_set_console_level(&wpm_logger, WSP_LOG_OFF);
    wsp_log_set_file_level(&wpm_logger, file_level);
#endif
    if (wpm_get_environment_variable("WPM_LOG_FILE", configured_path,
            sizeof(configured_path))) {
#ifdef WPM_NATIVE_OPERATIONAL_LOG
        selected_path = configured_path;
#else
        if (wsp_log_open_file(&wpm_logger, configured_path, 1) != 0) return 0;
#endif
    }
    else {
        if (!wpm_get_data_root(data_root, sizeof(data_root)) ||
            snprintf(log_directory, sizeof(log_directory), "%s\\audit", data_root) <= 0 ||
            (!CreateDirectoryA(log_directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) ||
            snprintf(log_path, sizeof(log_path), "%s\\audit\\wpm.log", data_root) <= 0) return 0;
#ifdef WPM_NATIVE_OPERATIONAL_LOG
        selected_path = log_path;
#else
        if (wsp_log_open_file(&wpm_logger, log_path, 1) != 0) return 0;
#endif
    }
#ifdef WPM_NATIVE_OPERATIONAL_LOG
    wpm_log_handle = CreateFileA(selected_path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (wpm_log_handle == INVALID_HANDLE_VALUE) return 0;
#endif
    wpm_logger_initialized = 1;
    atexit(wpm_log_close);
    return 1;
}

void wpm_log_close(void)
{
#ifdef WPM_NATIVE_OPERATIONAL_LOG
    if (wpm_log_handle != INVALID_HANDLE_VALUE) CloseHandle(wpm_log_handle);
    wpm_log_handle = INVALID_HANDLE_VALUE;
#else
    if (wpm_logger_initialized) wsp_log_close(&wpm_logger);
#endif
    wpm_logger_initialized = 0;
}

int wpm_vprintf(const char* format, va_list arguments)
{
    char message[8192];
    size_t length;
    int result;
    if (vsnprintf(message, sizeof(message), format, arguments) < 0) return -1;
    message[sizeof(message) - 1] = '\0';
    result = wpm_write_console_message(message, wpm_message_style_for(message));
#ifdef WPM_NATIVE_OPERATIONAL_LOG
    wpm_native_log_write(message);
#else
    if (wpm_logger_initialized) {
        length = strlen(message);
        while (length > 0 && (message[length - 1] == '\n' || message[length - 1] == '\r')) {
            message[--length] = '\0';
        }
        if (length > 0) {
            wsp_log_write(&wpm_logger, wpm_log_level_for_message(message), "%s", message);
        }
    }
#endif
    return result;
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
