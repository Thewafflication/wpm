#ifndef WPM_MINIZIP_COMPAT_H
#define WPM_MINIZIP_COMPAT_H

/* Prevent TinyCC's MinGW compatibility headers from redefining WCRT types. */
#define _TIME_T_DEFINED
#define _WCTYPE_T_DEFINED
#define _WINT_T

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static char* wpm_minizip_strdup(const char* source) {
    size_t length = strlen(source) + 1;
    char* copy = (char*)malloc(length);
    if (copy) memcpy(copy, source, length);
    return copy;
}

static struct tm* wpm_minizip_localtime_r(const time_t* timer, struct tm* result) {
    struct tm* current = localtime(timer);
    if (!current) return NULL;
    *result = *current;
    return result;
}

static int wpm_minizip_wstati64(const wchar_t* path, struct _stat64* result) {
    if (!path || !path[0] || !result) return -1;
    memset(result, 0, sizeof(*result));
    result->st_dev = (unsigned short)path[0];
    return 0;
}

#define strdup wpm_minizip_strdup
#define localtime_r wpm_minizip_localtime_r
#define _stati64 _stat64
#define _wstati64 wpm_minizip_wstati64

#endif
