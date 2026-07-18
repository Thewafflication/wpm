#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "helpers.h"

FILE* wpm_fopen(const char* path, const char* mode) {
    FILE* file = NULL;
#ifdef _MSC_VER
    if (fopen_s(&file, path, mode) != 0) return NULL;
#else
    file = fopen(path, mode);
#endif
    return file;
}

int wpm_get_environment_variable(const char* name, char* result, size_t result_size) {
#ifdef _WIN32
    DWORD length = GetEnvironmentVariableA(name, result, (DWORD)result_size);
    return length > 0 && length < result_size;
#else
    const char* value = getenv(name);
    if (!value || !value[0] || strlen(value) >= result_size) return 0;
    memcpy(result, value, strlen(value) + 1);
    return 1;
#endif
}

int file_exists(const char* path) {
    FILE* f = wpm_fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}
