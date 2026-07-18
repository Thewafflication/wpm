#ifndef WPM_HELPERS_H
#define WPM_HELPERS_H

#include <stdio.h>

int file_exists(const char* path);
FILE* wpm_fopen(const char* path, const char* mode);
int wpm_get_environment_variable(const char* name, char* result, size_t result_size);

#endif
