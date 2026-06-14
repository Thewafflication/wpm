#ifndef WPM_ARCHIVE_H
#define WPM_ARCHIVE_H

int wpm_archive_build(const char* source_dir, const char* output_dir);
int wpm_archive_extract(const char* archive_path, const char* destination_dir);
int wpm_archive_install(const char* archive_path);

#endif
