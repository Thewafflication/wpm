#ifndef WPM_ARCHIVE_H
#define WPM_ARCHIVE_H

void wpm_set_verbose(int enabled);
int wpm_archive_build(const char* source_dir, const char* output_dir, int update_index);
int wpm_archive_extract(const char* archive_path, const char* destination_dir);
int wpm_archive_install(const char* archive_path);
int wpm_archive_remove(const char* package_name);

#endif
