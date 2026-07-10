#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "archive.h"
#include "miniz.h"
#include "sodium.h"

#define WPM_PATH_SIZE 4096
#define WPM_INSTALL_ROOT "C:\\TEMP"
#define WPM_MAX_IGNORE_PATTERNS 128
#define WPM_BLAKE2B_BYTES crypto_generichash_BYTES
#define WPM_BLAKE2B_HEX_SIZE (WPM_BLAKE2B_BYTES * 2 + 1)

typedef struct wpm_ignore_list {
    char patterns[WPM_MAX_IGNORE_PATTERNS][WPM_PATH_SIZE];
    size_t count;
} wpm_ignore_list;

static const char* path_basename(const char* path);
static int normalized_full_path(const char* path, char* result, size_t result_size);

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

static void normalize_archive_separators(char* path) {
    for (char* current = path; *current; current++) {
        if (*current == '\\') *current = '/';
    }
}

static void trim_line(char* line) {
    size_t length = strlen(line);

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r' ||
        line[length - 1] == ' ' || line[length - 1] == '\t')) {
        line[--length] = '\0';
    }

    char* first = line;
    while (*first == ' ' || *first == '\t') first++;
    if (first != line) memmove(line, first, strlen(first) + 1);
}

static int wildcard_match(const char* pattern, const char* text) {
    while (*pattern) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) return 1;
            while (*text) {
                if (wildcard_match(pattern, text)) return 1;
                text++;
            }
            return wildcard_match(pattern, text);
        }
        if (*pattern != *text) return 0;
        pattern++;
        text++;
    }

    return *text == '\0';
}

static int load_ignore_list(const char* source_dir, wpm_ignore_list* ignore_list) {
    char ignore_path[WPM_PATH_SIZE];
    FILE* file;

    ignore_list->count = 0;
    if (!join_path(ignore_path, sizeof(ignore_path), source_dir, ".wpm\\wpmignore.txt")) return 0;

    file = fopen(ignore_path, "r");
    if (!file) return 1;

    while (ignore_list->count < WPM_MAX_IGNORE_PATTERNS &&
        fgets(ignore_list->patterns[ignore_list->count], WPM_PATH_SIZE, file)) {
        trim_line(ignore_list->patterns[ignore_list->count]);
        normalize_archive_separators(ignore_list->patterns[ignore_list->count]);
        if (ignore_list->patterns[ignore_list->count][0] == '\0' ||
            ignore_list->patterns[ignore_list->count][0] == '#') {
            continue;
        }
        ignore_list->count++;
    }

    fclose(file);
    return 1;
}

static int ignore_pattern_matches(const char* pattern, const char* archive_path) {
    size_t pattern_length = strlen(pattern);
    const char* basename = path_basename(archive_path);

    if (pattern_length == 0) return 0;

    if (pattern[pattern_length - 1] == '/') {
        return _strnicmp(pattern, archive_path, pattern_length) == 0;
    }

    if (strchr(pattern, '*')) {
        return wildcard_match(pattern, archive_path) || wildcard_match(pattern, basename);
    }

    return _stricmp(pattern, archive_path) == 0 || _stricmp(pattern, basename) == 0;
}

static int is_ignored_by_list(const wpm_ignore_list* ignore_list, const char* archive_path) {
    for (size_t i = 0; i < ignore_list->count; i++) {
        if (ignore_pattern_matches(ignore_list->patterns[i], archive_path)) return 1;
    }

    return 0;
}

static int ensure_sodium_ready(void) {
    static int initialized = 0;

    if (initialized) return 1;
    if (sodium_init() < 0) {
        printf("Error: could not initialize libsodium.\n");
        return 0;
    }

    initialized = 1;
    return 1;
}

static int calculate_file_blake2b(const char* path, char* hex, size_t hex_size) {
    unsigned char buffer[8192];
    unsigned char hash[WPM_BLAKE2B_BYTES];
    crypto_generichash_state state;
    FILE* file = fopen(path, "rb");

    if (!file) return 0;
    if (!ensure_sodium_ready() ||
        crypto_generichash_init(&state, NULL, 0, sizeof(hash)) != 0) {
        fclose(file);
        return 0;
    }

    for (;;) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
        if (bytes_read > 0) {
            if (crypto_generichash_update(&state, buffer, bytes_read) != 0) {
                fclose(file);
                return 0;
            }
        }
        if (bytes_read < sizeof(buffer)) {
            if (ferror(file)) {
                fclose(file);
                return 0;
            }
            break;
        }
    }

    if (crypto_generichash_final(&state, hash, sizeof(hash)) != 0) {
        fclose(file);
        return 0;
    }

    sodium_bin2hex(hex, hex_size, hash, sizeof(hash));
    return fclose(file) == 0;
}

static int get_file_size_bytes(const char* path, unsigned long long* size) {
    LARGE_INTEGER file_size;
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(file, &file_size)) {
        CloseHandle(file);
        return 0;
    }

    CloseHandle(file);
    *size = (unsigned long long)file_size.QuadPart;
    return 1;
}

static int write_index_entries(
    FILE* index,
    const char* source_dir,
    const char* archive_dir,
    const char* output_archive,
    const wpm_ignore_list* ignore_list
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

        normalize_archive_separators(archive_path);
        if (_stricmp(archive_path, ".wpm/index.csv") == 0 ||
            is_ignored_by_list(ignore_list, archive_path)) {
            continue;
        }

        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!write_index_entries(index, source_path, archive_path, output_archive, ignore_list)) {
                FindClose(search);
                return 0;
            }
        }
        else {
            char source_full_path[WPM_PATH_SIZE];
            char blake2b[WPM_BLAKE2B_HEX_SIZE];
            unsigned long long file_size;

            if (!normalized_full_path(source_path, source_full_path, sizeof(source_full_path))) {
                FindClose(search);
                return 0;
            }
            if (_stricmp(source_full_path, output_archive) == 0) continue;

            if (!get_file_size_bytes(source_path, &file_size) ||
                !calculate_file_blake2b(source_path, blake2b, sizeof(blake2b)) ||
                fprintf(index, "%s,%llu,%s,blake2b\n", archive_path, file_size, blake2b) < 0) {
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

static int update_package_index(const char* source_dir, const char* output_archive) {
    char wpm_dir[WPM_PATH_SIZE];
    char index_path[WPM_PATH_SIZE];
    FILE* index;
    wpm_ignore_list ignore_list;
    int success;

    if (!join_path(wpm_dir, sizeof(wpm_dir), source_dir, ".wpm") ||
        !join_path(index_path, sizeof(index_path), source_dir, ".wpm\\index.csv")) {
        printf("Error: package index path is too long.\n");
        return 0;
    }

    if (!create_directories(wpm_dir)) {
        printf("Error: could not create package metadata directory: %s\n", wpm_dir);
        return 0;
    }

    if (!load_ignore_list(source_dir, &ignore_list)) {
        printf("Error: could not read .wpmignore file.\n");
        return 0;
    }

    index = fopen(index_path, "w");
    if (!index) {
        printf("Error: could not write package index: %s\n", index_path);
        return 0;
    }

    success = fprintf(index, "filename,size,hash,algorithm\n") >= 0 &&
        write_index_entries(index, source_dir, "", output_archive, &ignore_list);

    if (fclose(index) != 0) success = 0;
    if (!success) {
        printf("Error: could not populate package index.\n");
        return 0;
    }

    return 1;
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

int wpm_archive_build(const char* source_dir, const char* output_dir, int update_index) {
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

    if (update_index && !update_package_index(source_full_path, output_path)) return 0;

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

static int file_exists_at_path(const char* path) {
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int verify_package_index(const char* destination_dir) {
    char index_path[WPM_PATH_SIZE];
    char line[WPM_PATH_SIZE + WPM_BLAKE2B_HEX_SIZE + 64];
    FILE* index;
    unsigned long line_number = 0;

    if (!join_path(index_path, sizeof(index_path), destination_dir, ".wpm\\index.csv")) {
        printf("Error: package index path is too long.\n");
        return 0;
    }

    if (!file_exists_at_path(index_path)) return 1;

    index = fopen(index_path, "r");
    if (!index) {
        printf("Error: could not open package index: %s\n", index_path);
        return 0;
    }

    while (fgets(line, sizeof(line), index)) {
        char* comma;
        char* second_comma;
        char* third_comma;
        char* filename;
        char* expected_size_text;
        char* expected_hash;
        char* algorithm;
        char actual_hash[WPM_BLAKE2B_HEX_SIZE];
        char relative_path[WPM_PATH_SIZE];
        char file_path[WPM_PATH_SIZE];
        unsigned long long expected_size;
        unsigned long long actual_size;
        char trailing;

        line_number++;
        trim_line(line);
        if (line_number == 1 && _stricmp(line, "filename,size,hash,algorithm") == 0) continue;
        if (line[0] == '\0') continue;

        comma = strchr(line, ',');
        if (!comma) {
            printf("Error: invalid package index entry at line %lu.\n", line_number);
            fclose(index);
            return 0;
        }
        second_comma = strchr(comma + 1, ',');
        third_comma = second_comma ? strchr(second_comma + 1, ',') : NULL;
        if (!second_comma || !third_comma) {
            printf("Error: invalid package index entry at line %lu.\n", line_number);
            fclose(index);
            return 0;
        }

        *comma = '\0';
        *second_comma = '\0';
        *third_comma = '\0';
        filename = line;
        expected_size_text = comma + 1;
        expected_hash = second_comma + 1;
        algorithm = third_comma + 1;
        trim_line(filename);
        trim_line(expected_size_text);
        trim_line(expected_hash);
        trim_line(algorithm);

        if (!is_safe_archive_path(filename) ||
            sscanf_s(expected_size_text, "%llu%c", &expected_size, &trailing, 1) != 1 ||
            strlen(expected_hash) != WPM_BLAKE2B_HEX_SIZE - 1 ||
            _stricmp(algorithm, "blake2b") != 0) {
            printf("Error: invalid package index entry at line %lu.\n", line_number);
            fclose(index);
            return 0;
        }

        strcpy_s(relative_path, sizeof(relative_path), filename);
        for (char* current = relative_path; *current; current++) {
            if (*current == '/') *current = '\\';
        }

        if (!join_path(file_path, sizeof(file_path), destination_dir, relative_path)) {
            printf("Error: indexed package path is too long.\n");
            fclose(index);
            return 0;
        }

        if (!file_exists_at_path(file_path) ||
            !get_file_size_bytes(file_path, &actual_size) ||
            actual_size != expected_size ||
            !calculate_file_blake2b(file_path, actual_hash, sizeof(actual_hash)) ||
            _stricmp(actual_hash, expected_hash) != 0) {
            printf("Error: package signature verification failed for %s.\n", filename);
            fclose(index);
            return 0;
        }
    }

    if (ferror(index)) {
        printf("Error: could not read package index.\n");
        fclose(index);
        return 0;
    }

    fclose(index);
    return 1;
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
    if (!verify_package_index(destination_path)) return 0;

    printf("Installed package to: %s\n", destination_path);
    return 1;
}
