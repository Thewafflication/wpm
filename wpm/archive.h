#ifndef WPM_ARCHIVE_H
#define WPM_ARCHIVE_H

#include <stddef.h>

typedef struct wpm_package_info {
    char name[128];
    char version[64];
    char arch[16];
    char archive_name[4096];
} wpm_package_info;

int wpm_get_data_root(char* result, size_t result_size);
int wpm_initialize_data_directories(void);
void wpm_set_verbose(int enabled);
int wpm_archive_build(const char* source_dir, const char* output_dir, int update_index, const char* private_key);
int wpm_archive_extract(const char* archive_path, const char* destination_dir);
int wpm_archive_install(const char* archive_path, int allow_unsigned);
int wpm_archive_inspect(const char* archive_path, wpm_package_info* info);
int wpm_archive_upgrade(const char* archive_path, int allow_unsigned,
    const char* expected_name, const char* expected_version,
    const char* expected_arch, const char* old_version);
int wpm_archive_schedule_self_upgrade(const char* archive_path, int allow_unsigned,
    const char* expected_version, const char* expected_arch,
    const char* old_version);
int wpm_archive_remove(const char* package_name);

#endif
