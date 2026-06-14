#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "archive.h"
#include "miniz.h"

#define WPM_PATH_SIZE 4096
#define WPM_INSTALL_ROOT "C:\\TEMP"

static int is_directory(const char* path) {
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int ensure_directory(const char* path) {
    if (is_directory(path)) return 1;
    if (CreateDirectoryA(path, NULL)) return 1;
    return is_directory(path);
}

static int create_directories(const char* path) {
    char partial[WPM_PATH_SIZE];
    size_t length = strlen(path);

    if (length == 0 || length >= sizeof(partial)) return 0;

    strcpy_s(partial, sizeof(partial), path);
    for (size_t i = 0; i < length; i++) {
        if (partial[i] == '/') partial[i] = '\\';
    }

    for (size_t i = 0; i < length; i++) {
        if (partial[i] != '\\' || i == 0 || (i == 2 && partial[1] == ':')) {
            continue;
        }

        partial[i] = '\0';
        if (!ensure_directory(partial)) return 0;
        partial[i] = '\\';
    }

    return ensure_directory(partial);
}

static int join_path(char* result, size_t result_size, const char* left, const char* right) {
    size_t left_length = strlen(left);
    int written;
    const char* separator =
        left_length > 0 && (left[left_length - 1] == '\\' || left[left_length - 1] == '/')
        ? ""
        : "\\";

    written = snprintf(result, result_size, "%s%s%s", left, separator, right);
    return written >= 0 && (size_t)written < result_size;
}

static const char* path_basename(const char* path) {
    const char* result = path;

    for (const char* current = path; *current; current++) {
        if (*current == '\\' || *current == '/') result = current + 1;
    }

    return result;
}

static int normalized_full_path(const char* path, char* result, size_t result_size) {
    DWORD length = GetFullPathNameA(path, (DWORD)result_size, result, NULL);
    return length > 0 && length < result_size;
}

static void trim_trailing_separators(char* path) {
    size_t length = strlen(path);

    while (length > 3 && (path[length - 1] == '\\' || path[length - 1] == '/')) {
        path[--length] = '\0';
    }
}

static int add_directory_to_zip(
    mz_zip_archive* zip,
    const char* source_dir,
    const char* archive_dir,
    const char* output_archive
) {
    char search_path[WPM_PATH_SIZE];
    WIN32_FIND_DATAA entry;
    HANDLE search;

    if (!join_path(search_path, sizeof(search_path), source_dir, "*")) return 0;

    search = FindFirstFileA(search_path, &entry);
    if (search == INVALID_HANDLE_VALUE) return 0;

    do {
        char source_path[WPM_PATH_SIZE];
        char archive_path[WPM_PATH_SIZE];

        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
            continue;
        }
        if (!join_path(source_path, sizeof(source_path), source_dir, entry.cFileName)) {
            FindClose(search);
            return 0;
        }

        if (archive_dir[0]) {
            int written = snprintf(
                    archive_path,
                    sizeof(archive_path),
                    "%s/%s",
                    archive_dir,
                    entry.cFileName
                );
            if (written < 0 || (size_t)written >= sizeof(archive_path)) {
                FindClose(search);
                return 0;
            }
        }
        else {
            strcpy_s(archive_path, sizeof(archive_path), entry.cFileName);
        }

        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            char directory_entry[WPM_PATH_SIZE];
            int written = snprintf(directory_entry, sizeof(directory_entry), "%s/", archive_path);
            if (written < 0 || (size_t)written >= sizeof(directory_entry) ||
                !mz_zip_writer_add_mem(zip, directory_entry, NULL, 0, MZ_BEST_COMPRESSION) ||
                !add_directory_to_zip(zip, source_path, archive_path, output_archive)) {
                FindClose(search);
                return 0;
            }
        }
        else {
            char source_full_path[WPM_PATH_SIZE];
            if (!normalized_full_path(source_path, source_full_path, sizeof(source_full_path))) {
                FindClose(search);
                return 0;
            }
            if (_stricmp(source_full_path, output_archive) == 0) continue;

            if (!mz_zip_writer_add_file(
                    zip,
                    archive_path,
                    source_path,
                    NULL,
                    0,
                    MZ_BEST_COMPRESSION
                )) {
                FindClose(search);
                return 0;
            }
        }
    } while (FindNextFileA(search, &entry));

    {
        DWORD error = GetLastError();
        FindClose(search);
        return error == ERROR_NO_MORE_FILES;
    }
}

int wpm_archive_build(const char* source_dir, const char* output_dir) {
    char source_full_path[WPM_PATH_SIZE];
    char output_full_path[WPM_PATH_SIZE];
    char output_path[WPM_PATH_SIZE];
    char archive_name[WPM_PATH_SIZE];
    const char* source_name;
    mz_zip_archive zip;
    int success = 0;

    if (!is_directory(source_dir)) {
        printf("Error: source directory not found: %s\n", source_dir);
        return 0;
    }
    if (!create_directories(output_dir)) {
        printf("Error: could not create output directory: %s\n", output_dir);
        return 0;
    }
    if (!normalized_full_path(source_dir, source_full_path, sizeof(source_full_path)) ||
        !normalized_full_path(output_dir, output_full_path, sizeof(output_full_path))) {
        printf("Error: source or output path is too long.\n");
        return 0;
    }
    trim_trailing_separators(source_full_path);
    trim_trailing_separators(output_full_path);

    source_name = path_basename(source_full_path);
    if (!source_name[0]) {
        printf("Error: source directory must have a package name.\n");
        return 0;
    }
    {
        int written = snprintf(archive_name, sizeof(archive_name), "%s.zip", source_name);
        if (written < 0 || (size_t)written >= sizeof(archive_name) ||
            !join_path(output_path, sizeof(output_path), output_full_path, archive_name)) {
            printf("Error: output path is too long.\n");
            return 0;
        }
    }

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, output_path, 0)) {
        printf("Error: could not create archive: %s\n", output_path);
        return 0;
    }

    if (add_directory_to_zip(&zip, source_full_path, "", output_path) &&
        mz_zip_writer_finalize_archive(&zip)) {
        success = 1;
    }
    mz_zip_writer_end(&zip);

    if (!success) {
        remove(output_path);
        printf("Error: failed while building archive.\n");
        return 0;
    }

    printf("Built package: %s\n", output_path);
    return 1;
}

static int is_safe_archive_path(const char* path) {
    const char* segment = path;

    if (!path[0] || path[0] == '/' || path[0] == '\\' || strchr(path, ':')) return 0;

    for (const char* current = path; ; current++) {
        if (*current == '/' || *current == '\\' || *current == '\0') {
            size_t segment_length = (size_t)(current - segment);
            if (segment_length == 2 && segment[0] == '.' && segment[1] == '.') return 0;
            if (*current == '\0') break;
            segment = current + 1;
        }
    }

    return 1;
}

static int create_parent_directory(const char* path) {
    char parent[WPM_PATH_SIZE];
    char* separator;

    strcpy_s(parent, sizeof(parent), path);
    separator = strrchr(parent, '\\');
    if (!separator) return 1;
    *separator = '\0';
    return create_directories(parent);
}

int wpm_archive_extract(const char* archive_path, const char* destination_dir) {
    mz_zip_archive zip;
    mz_uint file_count;
    int success = 0;

    if (!create_directories(destination_dir)) {
        printf("Error: could not create extraction directory: %s\n", destination_dir);
        return 0;
    }

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, archive_path, 0)) {
        printf("Error: could not open package archive: %s\n", archive_path);
        return 0;
    }

    file_count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat stat;
        char relative_path[WPM_PATH_SIZE];
        char destination_path[WPM_PATH_SIZE];

        if (!mz_zip_reader_file_stat(&zip, i, &stat) ||
            !is_safe_archive_path(stat.m_filename)) {
            printf("Error: package contains an invalid path.\n");
            goto cleanup;
        }

        strcpy_s(relative_path, sizeof(relative_path), stat.m_filename);
        for (char* current = relative_path; *current; current++) {
            if (*current == '/') *current = '\\';
        }
        if (!join_path(
                destination_path,
                sizeof(destination_path),
                destination_dir,
                relative_path
            )) {
            printf("Error: extracted path is too long.\n");
            goto cleanup;
        }

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            if (!create_directories(destination_path)) {
                printf("Error: could not create directory: %s\n", destination_path);
                goto cleanup;
            }
        }
        else {
            if (!create_parent_directory(destination_path) ||
                !mz_zip_reader_extract_to_file(&zip, i, destination_path, 0)) {
                printf("Error: could not extract file: %s\n", destination_path);
                goto cleanup;
            }
        }
    }

    success = 1;

cleanup:
    mz_zip_reader_end(&zip);
    return success;
}

int wpm_archive_install(const char* archive_path) {
    char archive_full_path[WPM_PATH_SIZE];
    char package_name[WPM_PATH_SIZE];
    char destination_path[WPM_PATH_SIZE];
    char* extension;

    if (!normalized_full_path(archive_path, archive_full_path, sizeof(archive_full_path))) {
        printf("Error: package path is too long: %s\n", archive_path);
        return 0;
    }

    strcpy_s(package_name, sizeof(package_name), path_basename(archive_full_path));
    extension = strrchr(package_name, '.');
    if (extension && _stricmp(extension, ".zip") == 0) *extension = '\0';
    if (!package_name[0]) {
        printf("Error: package archive must have a name.\n");
        return 0;
    }

    if (!join_path(destination_path, sizeof(destination_path), WPM_INSTALL_ROOT, package_name)) {
        printf("Error: installation path is too long.\n");
        return 0;
    }

    if (!wpm_archive_extract(archive_full_path, destination_path)) return 0;

    printf("Installed package to: %s\n", destination_path);
    return 1;
}
