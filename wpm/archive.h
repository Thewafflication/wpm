#ifndef WPM_ARCHIVE_H
#define WPM_ARCHIVE_H

#include <stddef.h>

int wpm_get_data_root(char* result, size_t result_size);
int wpm_initialize_data_directories(void);
void wpm_set_verbose(int enabled);
int wpm_archive_build(const char* source_dir, const char* output_dir, int update_index);
int wpm_archive_extract(const char* archive_path, const char* destination_dir);
int wpm_archive_install(const char* archive_path);
int wpm_archive_remove(const char* package_name);

#endif
