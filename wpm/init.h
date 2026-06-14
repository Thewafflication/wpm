#ifndef WPM_INIT_H
#define WPM_INIT_H

#include <stddef.h>

int wpm_current_directory_name(char* name, size_t size);
int wpm_package_name_is_valid(const char* name);
int wpm_init_run(const char* name);

#endif
