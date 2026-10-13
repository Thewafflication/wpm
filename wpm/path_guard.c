/** @file path_guard.c @brief Windows directory identity boundary.
 * At most 128 ancestors are held without delete sharing. Opening checks each
 * component through its own handle before proceeding beneath it. Missing
 * directories are created only in the explicit write-capable bootstrap mode.
 * Failure retains a releasable partial guard and never follows a reparse root.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "path_guard.h"
#include "capability.h"

struct wpm_path_guard {
    HANDLE handles[128];
    size_t count;
};

wpm_path_result wpm_path_guard_open(const char *path, int create,
                                    char *canonical, size_t size,
                                    wpm_path_guard **result) {
    wpm_path_guard *guard;
    DWORD length;
    if (!result) {
        return WPM_PATH_FAILED;
    }
    *result = NULL;
    if (!path || !canonical || size > MAXDWORD || create < 0 || create > 2 ||
        (create &&
         !wpm_require_capability(WPM_CAP_WRITE, "directory bootstrap"))) {
        return WPM_PATH_FAILED;
    }
    length = GetFullPathNameA(path, (DWORD)size, canonical, NULL);
    if (!length || length >= size) {
        return WPM_PATH_FAILED;
    }
    while (length > 3 && canonical[length - 1] == '\\') {
        canonical[--length] = 0;
    }
    if (length < 3 || !isalpha((unsigned char)canonical[0]) ||
        canonical[1] != ':' || canonical[2] != '\\') {
        return WPM_PATH_UNSAFE;
    }
    for (size_t i = 3; i <= length; i++) {
        unsigned char value = (unsigned char)canonical[i];
        if (value == ':' || (value && (value < 32 || value == 127)) ||
            ((i == length || value == '\\') &&
             (canonical[i - 1] == '.' || canonical[i - 1] == ' '))) {
            return WPM_PATH_UNSAFE;
        }
    }
    guard = calloc(1, sizeof(*guard));
    if (!guard) {
        return WPM_PATH_FAILED;
    }
    *result = guard;
    for (size_t i = 3; i <= length; i++) {
        char saved;
        HANDLE directory;
        BY_HANDLE_FILE_INFORMATION info;
        DWORD attributes, error;
        if (i != 3 && i != length && canonical[i] != '\\') {
            continue;
        }
        if (guard->count == 128) {
            return WPM_PATH_UNSAFE;
        }
        saved = canonical[i];
        canonical[i] = 0;
        attributes = GetFileAttributesA(canonical);
        error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0;
        if (create == 2 && i == length &&
            attributes != INVALID_FILE_ATTRIBUTES) {
            canonical[i] = saved;
            return WPM_PATH_UNSAFE;
        }
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            if (!create) {
                canonical[i] = saved;
                return WPM_PATH_ABSENT;
            }
            if (!CreateDirectoryA(canonical, NULL)) {
                DWORD creation_error = GetLastError();
                if (creation_error != ERROR_ALREADY_EXISTS ||
                    (create == 2 && i == length)) {
                    canonical[i] = saved;
                    return WPM_PATH_FAILED;
                }
            }
        }
        directory = CreateFileA(
            canonical, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        canonical[i] = saved;
        if (directory == INVALID_HANDLE_VALUE) {
            return WPM_PATH_FAILED;
        }
        guard->handles[guard->count++] = directory;
        if (!GetFileInformationByHandle(directory, &info) ||
            !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            return WPM_PATH_UNSAFE;
        }
    }
    return WPM_PATH_OPEN;
}

void wpm_path_guard_close(wpm_path_guard *guard) {
    if (guard) {
        while (guard->count) {
            CloseHandle(guard->handles[--guard->count]);
        }
        free(guard);
    }
}
