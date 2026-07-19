#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <windows.h>

#include "archive.h"
#include "helpers.h"
#include "miniz.h"
#include "sodium.h"
#include "signing.h"

#define WPM_PATH_SIZE 4096
#define WPM_DEFAULT_DATA_ROOT "C:\\ProgramData\\WPM"
#define WPM_MAX_IGNORE_PATTERNS 128
#define WPM_BLAKE2B_BYTES crypto_generichash_BYTES
#define WPM_BLAKE2B_HEX_SIZE (WPM_BLAKE2B_BYTES * 2 + 1)

typedef struct wpm_ignore_list {
    char patterns[WPM_MAX_IGNORE_PATTERNS][WPM_PATH_SIZE];
    size_t count;
} wpm_ignore_list;

typedef struct wpm_package_metadata {
    char name[128];
    char version[64];
    char arch[64];
    int debug;
} wpm_package_metadata;

static int wpm_verbose = 0;

static ULONGLONG wpm_tick_count(void) {
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600
    return (ULONGLONG)GetTickCount();
#else
    return GetTickCount64();
#endif
}

void wpm_set_verbose(int enabled) {
    wpm_verbose = enabled != 0;
}

static void verbose_log(const char* format, ...) {
    va_list arguments;

    if (!wpm_verbose) return;
    va_start(arguments, format);
    printf("  ");
    vprintf(format, arguments);
    printf("\n");
    va_end(arguments);
}

static const char* path_basename(const char* path);
static int normalized_full_path(const char* path, char* result, size_t result_size);
static int join_path(char* result, size_t result_size, const char* left, const char* right);

int wpm_get_data_root(char* result, size_t result_size) {
    char configured_root[WPM_PATH_SIZE];
    char program_data[WPM_PATH_SIZE];

    if (wpm_get_environment_variable("WPM_DATA_DIR", configured_root, sizeof(configured_root))) {
        return strcpy_s(result, result_size, configured_root) == 0;
    }
    if (wpm_get_environment_variable("ProgramData", program_data, sizeof(program_data))) {
        return join_path(result, result_size, program_data, "WPM");
    }
    return strcpy_s(result, result_size, WPM_DEFAULT_DATA_ROOT) == 0;
}

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

int wpm_initialize_data_directories(void) {
    char data_root[WPM_PATH_SIZE];
    char temp_root[WPM_PATH_SIZE];
    char package_root[WPM_PATH_SIZE];
    char cache_root[WPM_PATH_SIZE];
    char config_root[WPM_PATH_SIZE];

    if (!wpm_get_data_root(data_root, sizeof(data_root)) ||
        !join_path(temp_root, sizeof(temp_root), data_root, "temp") ||
        !join_path(package_root, sizeof(package_root), data_root, "packages") ||
        !join_path(cache_root, sizeof(cache_root), data_root, "cache") ||
        !join_path(config_root, sizeof(config_root), data_root, "config") ||
        !create_directories(data_root) || !create_directories(temp_root) ||
        !create_directories(package_root) || !create_directories(cache_root) ||
        !create_directories(config_root)) {
        printf("Error: could not initialize WPM data directories.\n");
        return 0;
    }

    verbose_log("WPM data directory: %s", data_root);
    return 1;
}

static int remove_directory_tree(const char* path) {
    char search_path[WPM_PATH_SIZE];
    WIN32_FIND_DATAA entry;
    HANDLE search;

    if (!is_directory(path)) return GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES;
    if (!join_path(search_path, sizeof(search_path), path, "*")) return 0;

    search = FindFirstFileA(search_path, &entry);
    if (search == INVALID_HANDLE_VALUE) return 0;

    do {
        char entry_path[WPM_PATH_SIZE];

        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) continue;
        if (!join_path(entry_path, sizeof(entry_path), path, entry.cFileName)) {
            FindClose(search);
            return 0;
        }
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
            if (!remove_directory_tree(entry_path)) {
                FindClose(search);
                return 0;
            }
        }
        else if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
            ? !RemoveDirectoryA(entry_path)
            : !DeleteFileA(entry_path)) {
            FindClose(search);
            return 0;
        }
    } while (FindNextFileA(search, &entry));

    {
        DWORD error = GetLastError();
        FindClose(search);
        return error == ERROR_NO_MORE_FILES && RemoveDirectoryA(path);
    }
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

    file = wpm_fopen(ignore_path, "r");
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

static int is_tracked_package_support_file(const char* archive_path) {
    return _stricmp(archive_path, ".wpm/package.txt") == 0 ||
        _stricmp(archive_path, ".wpm/install.cmd") == 0 ||
        _stricmp(archive_path, ".wpm/remove.cmd") == 0 ||
        _stricmp(archive_path, ".wpm/wpmignore.txt") == 0;
}

static int is_safe_metadata_value(const char* value) {
    if (!value || !value[0] || strcmp(value, ".") == 0 || strcmp(value, "..") == 0) return 0;

    for (const unsigned char* current = (const unsigned char*)value; *current; current++) {
        if (!isalnum(*current) && *current != '-' && *current != '_' && *current != '.' && *current != '+') {
            return 0;
        }
    }

    return 1;
}

static int parse_bool_metadata_value(const char* value, int* result) {
    if (_stricmp(value, "true") == 0 || _stricmp(value, "yes") == 0 ||
        _stricmp(value, "on") == 0 || strcmp(value, "1") == 0) {
        *result = 1;
        return 1;
    }
    if (_stricmp(value, "false") == 0 || _stricmp(value, "no") == 0 ||
        _stricmp(value, "off") == 0 || strcmp(value, "0") == 0) {
        *result = 0;
        return 1;
    }

    return 0;
}

static int read_package_metadata(const char* source_dir, wpm_package_metadata* metadata) {
    char package_path[WPM_PATH_SIZE];
    char line[WPM_PATH_SIZE];
    FILE* file;

    metadata->name[0] = '\0';
    metadata->version[0] = '\0';
    metadata->arch[0] = '\0';
    metadata->debug = 0;

    if (!join_path(package_path, sizeof(package_path), source_dir, ".wpm\\package.txt")) {
        printf("Error: package metadata path is too long.\n");
        return 0;
    }

    file = wpm_fopen(package_path, "r");
    if (!file) {
        printf("Error: could not open package metadata: %s\n", package_path);
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        char* equals;
        char* key;
        char* value;

        trim_line(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        key = line;
        value = equals + 1;
        trim_line(key);
        trim_line(value);

        if (_stricmp(key, "name") == 0) {
            if (strlen(value) >= sizeof(metadata->name)) {
                printf("Error: package name metadata is too long.\n");
                fclose(file);
                return 0;
            }
            strcpy_s(metadata->name, sizeof(metadata->name), value);
        }
        else if (_stricmp(key, "version") == 0) {
            if (strlen(value) >= sizeof(metadata->version)) {
                printf("Error: package version metadata is too long.\n");
                fclose(file);
                return 0;
            }
            strcpy_s(metadata->version, sizeof(metadata->version), value);
        }
        else if (_stricmp(key, "arch") == 0) {
            if (strlen(value) >= sizeof(metadata->arch)) {
                printf("Error: package arch metadata is too long.\n");
                fclose(file);
                return 0;
            }
            strcpy_s(metadata->arch, sizeof(metadata->arch), value);
        }
        else if (_stricmp(key, "debug") == 0 && !parse_bool_metadata_value(value, &metadata->debug)) {
            printf("Error: package metadata debug must be true or false.\n");
            fclose(file);
            return 0;
        }
    }

    if (ferror(file)) {
        printf("Error: could not read package metadata.\n");
        fclose(file);
        return 0;
    }
    fclose(file);

    if (!is_safe_metadata_value(metadata->name) ||
        !is_safe_metadata_value(metadata->version) ||
        !is_safe_metadata_value(metadata->arch)) {
        printf("Error: package metadata requires safe name, version, and arch values.\n");
        return 0;
    }

    return 1;
}

static int write_installation_audit(const char* data_root, const char* archive_name,
                                    const wpm_package_metadata* metadata, const char* signing_key_id) {
    char audit_dir[WPM_PATH_SIZE];
    char audit_path[WPM_PATH_SIZE];
    SYSTEMTIME now;
    FILE* file;
    int written;

    if (!join_path(audit_dir, sizeof(audit_dir), data_root, "audit") || !create_directories(audit_dir)) return 0;
    GetSystemTime(&now);
    written = snprintf(audit_path, sizeof(audit_path),
        "%s\\%04u%02u%02uT%02u%02u%02u.%03uZ-%lu-%s.install.txt",
        audit_dir, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, (unsigned long)GetCurrentProcessId(), metadata->name);
    if (written < 0 || (size_t)written >= sizeof(audit_path) || (file = wpm_fopen(audit_path, "wb")) == NULL) return 0;
    fprintf(file,
        "name=%s\nversion=%s\narchive=%s\ntimestamp=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\n"
        "signing-key=%s\nverification=verified\n",
        metadata->name, metadata->version, archive_name,
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
        signing_key_id);
    return fclose(file) == 0;
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
    FILE* file;

    verbose_log("Computing BLAKE2b hash: %s", path);
    file = wpm_fopen(path, "rb");

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
    DWORD file_size_low;
    DWORD file_size_high = 0;
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
    SetLastError(NO_ERROR);
    file_size_low = GetFileSize(file, &file_size_high);
    if (file_size_low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
        CloseHandle(file);
        return 0;
    }

    CloseHandle(file);
    *size = ((unsigned long long)file_size_high << 32) | file_size_low;
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
        if (_stricmp(archive_path, ".wpm/index.csv") == 0 || _stricmp(archive_path, ".wpm/signature.json") == 0 ||
            (!is_tracked_package_support_file(archive_path) &&
                is_ignored_by_list(ignore_list, archive_path))) {
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

            verbose_log("Indexing file: %s", archive_path);

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

    index = wpm_fopen(index_path, "w");
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

            verbose_log("Adding file to archive: %s", archive_path);

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

int wpm_archive_build(const char* source_dir, const char* output_dir, int update_index, const char* private_key) {
    char source_full_path[WPM_PATH_SIZE];
    char output_full_path[WPM_PATH_SIZE];
    char output_path[WPM_PATH_SIZE];
    char archive_name[WPM_PATH_SIZE];
    wpm_package_metadata metadata;
    mz_zip_archive zip;
    int success = 0;

    verbose_log("Building package from: %s", source_dir);

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

    verbose_log("Reading package metadata: %s\\.wpm\\package.txt", source_full_path);
    if (!read_package_metadata(source_full_path, &metadata)) return 0;
    {
        int written = snprintf(
            archive_name,
            sizeof(archive_name),
            metadata.debug ? "%s-%s-debug-%s.zip" : "%s-%s-%s.zip",
            metadata.name,
            metadata.arch,
            metadata.version
        );
        if (written < 0 || (size_t)written >= sizeof(archive_name) ||
            !join_path(output_path, sizeof(output_path), output_full_path, archive_name)) {
            printf("Error: output path is too long.\n");
            return 0;
        }
    }

    if (update_index) {
        verbose_log("Writing package index: %s\\.wpm\\index.csv", source_full_path);
        if (!update_package_index(source_full_path, output_path)) return 0;
    }
    if (private_key && private_key[0] && (!update_index || !wpm_sign_package_index(source_full_path, private_key))) {
        printf("Error: package signing requires a valid generated index.\n");
        return 0;
    }

    verbose_log("Creating archive: %s", output_path);

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

    verbose_log("Extracting archive: %s", archive_path);

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
            verbose_log("Creating directory: %s", destination_path);
            if (!create_directories(destination_path)) {
                printf("Error: could not create directory: %s\n", destination_path);
                goto cleanup;
            }
        }
        else {
            verbose_log("Extracting file: %s", destination_path);
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

static int index_contains_path(const char* index_path, const char* archive_path) {
    char line[WPM_PATH_SIZE + WPM_BLAKE2B_HEX_SIZE + 64];
    FILE* index = wpm_fopen(index_path, "r");
    if (!index) return 0;

    while (fgets(line, sizeof(line), index)) {
        char* comma;
        trim_line(line);
        comma = strchr(line, ',');
        if (comma) *comma = '\0';
        normalize_archive_separators(line);
        if (_stricmp(line, archive_path) == 0) {
            fclose(index);
            return 1;
        }
    }

    fclose(index);
    return 0;
}

static int verify_index_completeness(const char* destination_dir, const char* relative_dir, const char* index_path) {
    char search_dir[WPM_PATH_SIZE];
    char search_pattern[WPM_PATH_SIZE];
    WIN32_FIND_DATAA entry;
    HANDLE search;

    if (!join_path(search_dir, sizeof(search_dir), destination_dir, relative_dir) ||
        !join_path(search_pattern, sizeof(search_pattern), search_dir, "*")) return 0;
    search = FindFirstFileA(search_pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) return 0;

    do {
        char relative_path[WPM_PATH_SIZE];
        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) continue;
        if (relative_dir[0]) {
            if (snprintf(relative_path, sizeof(relative_path), "%s/%s", relative_dir, entry.cFileName) < 0) {
                FindClose(search);
                return 0;
            }
        } else {
            strcpy_s(relative_path, sizeof(relative_path), entry.cFileName);
        }
        normalize_archive_separators(relative_path);
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!verify_index_completeness(destination_dir, relative_path, index_path)) {
                FindClose(search);
                return 0;
            }
        } else if (_stricmp(relative_path, ".wpm/index.csv") != 0 &&
                   _stricmp(relative_path, ".wpm/signature.json") != 0 &&
                   !index_contains_path(index_path, relative_path)) {
            printf("Error: package contains unindexed file: %s.\n", relative_path);
            FindClose(search);
            return 0;
        }
    } while (FindNextFileA(search, &entry));

    FindClose(search);
    return 1;
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

    verbose_log("Verifying package index: %s", index_path);

    index = wpm_fopen(index_path, "r");
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

        verbose_log("Verifying file: %s", filename);

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
    {
        char signature_path[WPM_PATH_SIZE];
        if (!join_path(signature_path, sizeof(signature_path), destination_dir, ".wpm\\signature.json")) return 0;
        return !file_exists_at_path(signature_path) ||
            verify_index_completeness(destination_dir, "", index_path);
    }
}

static int is_valid_package_name(const char* package_name) {
    if (!package_name[0] || strcmp(package_name, ".") == 0 || strcmp(package_name, "..") == 0) {
        return 0;
    }

    for (const char* current = package_name; *current; current++) {
        if (*current == '\\' || *current == '/' || *current == ':') return 0;
    }

    return 1;
}

static int run_package_script(
    const char* staging_dir,
    const char* script_name,
    const char* action_name,
    DWORD* result_exit_code
) {
    char script_path[WPM_PATH_SIZE];
    char command_line[WPM_PATH_SIZE * 2];
    char self_upgrade_log[WPM_PATH_SIZE];
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;
    DWORD exit_code;
    int written;

    if (!join_path(script_path, sizeof(script_path), staging_dir, script_name)) {
        printf("Error: %s script path is too long.\n", action_name);
        return 0;
    }
    if (result_exit_code) *result_exit_code = 0;
    if (!file_exists_at_path(script_path)) return 1;

    verbose_log("Running %s script: %s", action_name, script_path);

    if (GetEnvironmentVariableA("WPM_SELF_UPGRADE_LOG", self_upgrade_log, sizeof(self_upgrade_log)) > 0) {
        written = snprintf(command_line, sizeof(command_line),
            "cmd.exe /d /s /c call \"%s\" >> \"%s\" 2>&1", script_path, self_upgrade_log);
    }
    else {
        written = snprintf(command_line, sizeof(command_line),
            "cmd.exe /d /s /c call \"%s\"", script_path);
    }
    if (written < 0 || (size_t)written >= sizeof(command_line)) {
        printf("Error: %s command is too long.\n", action_name);
        return 0;
    }

    memset(&startup_info, 0, sizeof(startup_info));
    memset(&process_info, 0, sizeof(process_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessA(
            NULL,
            command_line,
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            staging_dir,
            &startup_info,
            &process_info
        )) {
        printf("Error: could not start %s script: %s\n", action_name, script_path);
        return 0;
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    if (!GetExitCodeProcess(process_info.hProcess, &exit_code)) exit_code = 1;
    if (result_exit_code) *result_exit_code = exit_code;
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    if (exit_code != 0) {
        printf("Error: %s script failed with exit code %lu.\n", action_name, (unsigned long)exit_code);
        return 0;
    }

    return 1;
}

int wpm_archive_inspect(const char* archive_path, wpm_package_info* info) {
    char root[WPM_PATH_SIZE], temp[WPM_PATH_SIZE], stage[WPM_PATH_SIZE];
    wpm_package_metadata metadata;
    int result = 0;
    if (!archive_path || !info || !wpm_get_data_root(root, sizeof(root)) ||
        !join_path(temp, sizeof(temp), root, "temp") ||
        snprintf(stage, sizeof(stage), "%s\\inspect-%lu-%llu", temp,
            (unsigned long)GetCurrentProcessId(), (unsigned long long)wpm_tick_count()) < 0 ||
        !create_directories(temp) || !remove_directory_tree(stage)) return 0;
    if (wpm_archive_extract(archive_path, stage) && read_package_metadata(stage, &metadata)) {
        strcpy_s(info->name, sizeof(info->name), metadata.name);
        strcpy_s(info->version, sizeof(info->version), metadata.version);
        strcpy_s(info->arch, sizeof(info->arch), metadata.arch);
        strcpy_s(info->archive_name, sizeof(info->archive_name), path_basename(archive_path));
        result = 1;
    }
    if (!remove_directory_tree(stage)) result = 0;
    return result;
}

int wpm_archive_verify(const char* archive_path) {
    char archive_full_path[WPM_PATH_SIZE];
    char data_root[WPM_PATH_SIZE], temp_root[WPM_PATH_SIZE], staging_path[WPM_PATH_SIZE];
    char signing_key_id[65];
    wpm_package_metadata metadata;
    int success = 0;

    verbose_log("Verifying archive: %s", archive_path);
    if (!normalized_full_path(archive_path, archive_full_path, sizeof(archive_full_path))) {
        printf("Error: package path is too long: %s\n", archive_path);
        return 0;
    }
    if (!wpm_get_data_root(data_root, sizeof(data_root)) ||
        !join_path(temp_root, sizeof(temp_root), data_root, "temp") ||
        snprintf(staging_path, sizeof(staging_path), "%s\\verify-%lu-%llu", temp_root,
            (unsigned long)GetCurrentProcessId(), (unsigned long long)wpm_tick_count()) < 0 ||
        !create_directories(temp_root) || !remove_directory_tree(staging_path)) {
        printf("Error: could not prepare package verification staging.\n");
        return 0;
    }

    if (!wpm_archive_extract(archive_full_path, staging_path)) goto cleanup;
    if (!wpm_validate_package_signature(staging_path, 0, signing_key_id, sizeof(signing_key_id))) goto cleanup;
    if (!verify_package_index(staging_path)) goto cleanup;
    if (!read_package_metadata(staging_path, &metadata)) goto cleanup;
    success = 1;

cleanup:
    if (!remove_directory_tree(staging_path)) {
        printf("Error: could not remove verification staging directory: %s\n", staging_path);
        success = 0;
    }
    if (!success) return 0;
    printf("Verified package: %s (%s %s %s; signing key %s)\n",
        path_basename(archive_full_path), metadata.name, metadata.arch, metadata.version, signing_key_id);
    return 1;
}

static int installed_architecture_is_compatible(const wpm_package_metadata* candidate) {
    char root[WPM_PATH_SIZE], store[WPM_PATH_SIZE], search[WPM_PATH_SIZE], archive[WPM_PATH_SIZE];
    WIN32_FIND_DATAA entry;
    HANDLE find;
    int candidate_any = _stricmp(candidate->arch, "any") == 0;
    if (!wpm_get_data_root(root, sizeof(root)) || !join_path(store, sizeof(store), root, "packages") ||
        !join_path(search, sizeof(search), store, "*.zip")) return 0;
    find = FindFirstFileA(search, &entry);
    if (find == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        wpm_package_info installed;
        int installed_any;
        if (!join_path(archive, sizeof(archive), store, entry.cFileName) ||
            !wpm_archive_inspect(archive, &installed) || _stricmp(installed.name, candidate->name) != 0) continue;
        installed_any = _stricmp(installed.arch, "any") == 0;
        if (candidate_any != installed_any) {
            printf("Error: package architecture conflicts with installed %s %s; remove the conflicting installation first.\n",
                installed.name, installed.arch);
            FindClose(find);
            return 0;
        }
    } while (FindNextFileA(find, &entry));
    FindClose(find);
    return 1;
}

int wpm_archive_install(const char* archive_path, int allow_unsigned) {
    char archive_full_path[WPM_PATH_SIZE];
    char package_name[WPM_PATH_SIZE];
    char data_root[WPM_PATH_SIZE];
    char temp_root[WPM_PATH_SIZE];
    char package_store[WPM_PATH_SIZE];
    char staging_path[WPM_PATH_SIZE];
    char stored_archive_path[WPM_PATH_SIZE];
    char signing_key_id[65];
    wpm_package_metadata metadata;
    char* extension;
    int success = 0;

    verbose_log("Installing archive: %s", archive_path);

    if (!normalized_full_path(archive_path, archive_full_path, sizeof(archive_full_path))) {
        printf("Error: package path is too long: %s\n", archive_path);
        return 0;
    }

    strcpy_s(package_name, sizeof(package_name), path_basename(archive_full_path));
    extension = strrchr(package_name, '.');
    if (extension && _stricmp(extension, ".zip") == 0) *extension = '\0';
    if (!is_valid_package_name(package_name)) {
        printf("Error: package archive must have a name.\n");
        return 0;
    }

    if (!wpm_get_data_root(data_root, sizeof(data_root)) ||
        !join_path(temp_root, sizeof(temp_root), data_root, "temp") ||
        !join_path(package_store, sizeof(package_store), data_root, "packages") ||
        !join_path(staging_path, sizeof(staging_path), temp_root, package_name) ||
        !join_path(stored_archive_path, sizeof(stored_archive_path), package_store, path_basename(archive_full_path))) {
        printf("Error: installation path is too long.\n");
        return 0;
    }

    if (!create_directories(temp_root) || !create_directories(package_store)) {
        printf("Error: could not create WPM data directories.\n");
        return 0;
    }
    if (!remove_directory_tree(staging_path)) {
        printf("Error: could not clear staging directory: %s\n", staging_path);
        return 0;
    }

    verbose_log("Using staging directory: %s", staging_path);

    if (!wpm_archive_extract(archive_full_path, staging_path)) goto cleanup;
    if (!wpm_validate_package_signature(staging_path, allow_unsigned, signing_key_id, sizeof(signing_key_id))) goto cleanup;
    if (!verify_package_index(staging_path)) goto cleanup;
    if (!read_package_metadata(staging_path, &metadata)) goto cleanup;
    if (!installed_architecture_is_compatible(&metadata)) goto cleanup;
    if (!run_package_script(staging_path, ".wpm\\install.cmd", "install", NULL)) goto cleanup;
    if (_stricmp(archive_full_path, stored_archive_path) != 0) {
        verbose_log("Storing archive: %s", stored_archive_path);
    }
    if (_stricmp(archive_full_path, stored_archive_path) != 0 &&
        !CopyFileA(archive_full_path, stored_archive_path, FALSE)) {
        printf("Error: could not store package archive: %s\n", stored_archive_path);
        goto cleanup;
    }
    if (!write_installation_audit(data_root, path_basename(archive_full_path), &metadata, signing_key_id)) {
        printf("Error: could not record package verification audit.\n");
        goto cleanup;
    }

    success = 1;

cleanup:
    if (!remove_directory_tree(staging_path)) {
        printf("Error: could not remove staging directory: %s\n", staging_path);
        success = 0;
    }
    if (!success) return 0;

    printf("Installed package; archive stored at: %s\n", stored_archive_path);
    return 1;
}

static int write_upgrade_audit(const char* data_root, const wpm_package_metadata* metadata,
    const char* old_version, const char* archive_name, const char* signing_key,
    int failed, const char* phase, DWORD exit_code) {
    char directory[WPM_PATH_SIZE], path[WPM_PATH_SIZE];
    SYSTEMTIME now;
    FILE* file;
    if (!join_path(directory, sizeof(directory), data_root, "audit") || !create_directories(directory)) return 0;
    GetSystemTime(&now);
    if (snprintf(path, sizeof(path), "%s\\%04u%02u%02uT%02u%02u%02u.%03uZ-%lu-%s.%s.txt",
        directory, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, (unsigned long)GetCurrentProcessId(), metadata->name,
        failed ? "upgrade-failed" : "upgrade") < 0 || (file = wpm_fopen(path, "wb")) == NULL) return 0;
    fprintf(file, "name=%s\narch=%s\nold-version=%s\nnew-version=%s\narchive=%s\n"
        "signing-key=%s\nverification=%s\nstatus=%s\n",
        metadata->name, metadata->arch, old_version, metadata->version, archive_name,
        signing_key && signing_key[0] ? signing_key : "unknown", failed ? "failed" : "verified",
        failed ? "failed" : "upgraded");
    if (failed) fprintf(file, "phase=%s\nexit-code=%lu\n", phase ? phase : "unknown", (unsigned long)exit_code);
    return fclose(file) == 0;
}

int wpm_archive_schedule_self_upgrade(const char* archive_path, int allow_unsigned,
    const char* expected_version, const char* expected_arch,
    const char* old_version) {
    char archive_full[WPM_PATH_SIZE], root[WPM_PATH_SIZE], temp[WPM_PATH_SIZE];
    char stage[WPM_PATH_SIZE], staged_exe[WPM_PATH_SIZE], cache[WPM_PATH_SIZE];
    char handoff_dir[WPM_PATH_SIZE], handoff_exe[WPM_PATH_SIZE], audit_dir[WPM_PATH_SIZE], log_path[WPM_PATH_SIZE];
    char signing_key[65] = "", command[WPM_PATH_SIZE * 2];
    wpm_package_metadata metadata;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    int result = 0;
    if (!normalized_full_path(archive_path, archive_full, sizeof(archive_full)) ||
        !wpm_get_data_root(root, sizeof(root)) || !join_path(temp, sizeof(temp), root, "temp") ||
        snprintf(stage, sizeof(stage), "%s\\self-upgrade-stage-%lu", temp,
            (unsigned long)GetCurrentProcessId()) < 0 ||
        !join_path(staged_exe, sizeof(staged_exe), stage, "wpm.exe") ||
        !join_path(cache, sizeof(cache), root, "cache") ||
        !join_path(handoff_dir, sizeof(handoff_dir), cache, "self-upgrade") ||
        !join_path(audit_dir, sizeof(audit_dir), root, "audit") ||
        snprintf(log_path, sizeof(log_path), "%s\\self-upgrade-%s-%s.log", audit_dir,
            expected_arch, expected_version) < 0 ||
        snprintf(handoff_exe, sizeof(handoff_exe), "%s\\wpm-%s-%s.exe",
            handoff_dir, expected_arch, expected_version) < 0 ||
        !create_directories(temp) || !create_directories(cache) || !create_directories(audit_dir) ||
        !create_directories(handoff_dir) || !remove_directory_tree(stage)) return 0;
    DeleteFileA(log_path);
    memset(&metadata, 0, sizeof(metadata));
    strcpy_s(metadata.name, sizeof(metadata.name), "wpm");
    strcpy_s(metadata.version, sizeof(metadata.version), expected_version);
    strcpy_s(metadata.arch, sizeof(metadata.arch), expected_arch);
    if (!wpm_archive_extract(archive_full, stage)) {
        write_upgrade_audit(root, &metadata, old_version, path_basename(archive_full), signing_key, 1, "self-upgrade-extraction", 0);
        goto cleanup;
    }
    if (!wpm_validate_package_signature(stage, allow_unsigned, signing_key, sizeof(signing_key)) ||
        !verify_package_index(stage) || !read_package_metadata(stage, &metadata)) {
        write_upgrade_audit(root, &metadata, old_version, path_basename(archive_full), signing_key, 1, "self-upgrade-validation", 0);
        goto cleanup;
    }
    if (_stricmp(metadata.name, "wpm") != 0 || strcmp(metadata.version, expected_version) != 0 ||
        _stricmp(metadata.arch, expected_arch) != 0 || !file_exists_at_path(staged_exe)) {
        printf("Error: WPM self-upgrade package metadata or executable does not match the selected candidate.\n");
        write_upgrade_audit(root, &metadata, old_version, path_basename(archive_full), signing_key, 1, "self-upgrade-metadata", 0);
        goto cleanup;
    }
    if (!CopyFileA(staged_exe, handoff_exe, FALSE)) {
        printf("Error: could not cache the WPM self-upgrade executable.\n");
        goto cleanup;
    }
    if (snprintf(command, sizeof(command), "\"%s\" --complete-self-upgrade \"%s\" %lu \"%s\" \"%s\" \"%s\" %d \"%s\"",
        handoff_exe, archive_full, (unsigned long)GetCurrentProcessId(), expected_version,
        expected_arch, old_version, allow_unsigned ? 1 : 0, log_path) < 0) goto cleanup;
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    if (!CreateProcessA(handoff_exe, command, NULL, NULL, FALSE,
        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &startup, &process)) {
        printf("Error: could not launch the cached WPM self-upgrade executable.\n");
        goto cleanup;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    printf("Scheduled WPM self-upgrade to %s; installation will continue after this process exits.\n", expected_version);
    printf("Self-upgrade output: %s\n", log_path);
    result = 1;
cleanup:
    if (!remove_directory_tree(stage)) result = 0;
    return result;
}

int wpm_archive_upgrade(const char* archive_path, int allow_unsigned,
    const char* expected_name, const char* expected_version,
    const char* expected_arch, const char* old_version) {
    char archive_full[WPM_PATH_SIZE], base[WPM_PATH_SIZE], root[WPM_PATH_SIZE];
    char temp[WPM_PATH_SIZE], store[WPM_PATH_SIZE], stage[WPM_PATH_SIZE], stored[WPM_PATH_SIZE];
    char signing_key[65] = "";
    wpm_package_metadata metadata;
    DWORD script_exit = 0;
    int success = 0;
    memset(&metadata, 0, sizeof(metadata));
    strcpy_s(metadata.name, sizeof(metadata.name), expected_name);
    strcpy_s(metadata.version, sizeof(metadata.version), expected_version);
    strcpy_s(metadata.arch, sizeof(metadata.arch), expected_arch);
    if (!normalized_full_path(archive_path, archive_full, sizeof(archive_full))) return 0;
    strcpy_s(base, sizeof(base), path_basename(archive_full));
    if (!wpm_get_data_root(root, sizeof(root)) || !join_path(temp, sizeof(temp), root, "temp") ||
        !join_path(store, sizeof(store), root, "packages") || !join_path(stage, sizeof(stage), temp, base) ||
        !join_path(stored, sizeof(stored), store, base) || !create_directories(temp) ||
        !create_directories(store) || !remove_directory_tree(stage)) return 0;
    if (!wpm_archive_extract(archive_full, stage)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "extraction", 0);
        goto cleanup;
    }
    if (!wpm_validate_package_signature(stage, allow_unsigned, signing_key, sizeof(signing_key))) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "signature-validation", 0);
        goto cleanup;
    }
    if (!verify_package_index(stage)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "index-validation", 0);
        goto cleanup;
    }
    if (!read_package_metadata(stage, &metadata)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "metadata", 0);
        goto cleanup;
    }
    if (_stricmp(metadata.name, expected_name) != 0 || strcmp(metadata.version, expected_version) != 0 ||
        _stricmp(metadata.arch, expected_arch) != 0) {
        printf("Error: downloaded package metadata does not match the selected repository entry.\n");
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "metadata", 0);
        goto cleanup;
    }
    if (!installed_architecture_is_compatible(&metadata)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "architecture", 0);
        goto cleanup;
    }
    if (!run_package_script(stage, ".wpm\\install.cmd", "upgrade install", &script_exit)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "install-script", script_exit);
        printf("Warning: package-maintainer recovery may be required.\n");
        goto cleanup;
    }
    if (_stricmp(archive_full, stored) != 0 && !CopyFileA(archive_full, stored, FALSE)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "archive-retention", 0);
        printf("Error: upgrade deployed software but archive retention failed; recovery is required.\n");
        goto cleanup;
    }
    if (!write_upgrade_audit(root, &metadata, old_version, base, signing_key, 0, NULL, 0)) {
        printf("Error: upgrade deployed software but audit recording failed; recovery is required.\n");
        goto cleanup;
    }
    success = 1;
cleanup:
    if (!remove_directory_tree(stage)) success = 0;
    if (!success) {
        return 0;
    }
    printf("Upgraded %s %s %s to %s.\n", expected_name, expected_arch, old_version, expected_version);
    return 1;
}

int wpm_archive_remove(const char* package_name) {
    char archive_name[WPM_PATH_SIZE];
    char data_root[WPM_PATH_SIZE];
    char temp_root[WPM_PATH_SIZE];
    char package_store[WPM_PATH_SIZE];
    char staging_path[WPM_PATH_SIZE];
    char stored_archive_name[WPM_PATH_SIZE];
    char stored_archive_path[WPM_PATH_SIZE];
    char* extension;
    int written;
    int success = 0;

    if (strlen(package_name) >= sizeof(archive_name)) {
        printf("Error: package name is too long.\n");
        return 0;
    }
    strcpy_s(archive_name, sizeof(archive_name), package_name);
    extension = strrchr(archive_name, '.');
    if (extension && _stricmp(extension, ".zip") == 0) *extension = '\0';
    if (!is_valid_package_name(archive_name)) {
        printf("Error: package name must be a stored archive name.\n");
        return 0;
    }

    written = snprintf(stored_archive_name, sizeof(stored_archive_name), "%s.zip", archive_name);
    if (!wpm_get_data_root(data_root, sizeof(data_root)) ||
        !join_path(temp_root, sizeof(temp_root), data_root, "temp") ||
        !join_path(package_store, sizeof(package_store), data_root, "packages") ||
        !join_path(staging_path, sizeof(staging_path), temp_root, archive_name) ||
        written < 0 || (size_t)written >= sizeof(stored_archive_name) ||
        !join_path(stored_archive_path, sizeof(stored_archive_path), package_store, stored_archive_name)) {
        printf("Error: removal path is too long.\n");
        return 0;
    }
    if (!file_exists_at_path(stored_archive_path)) {
        printf("Error: stored package archive not found: %s\n", stored_archive_path);
        return 0;
    }
    if (!create_directories(temp_root) || !remove_directory_tree(staging_path)) {
        printf("Error: could not prepare removal staging directory.\n");
        return 0;
    }

    if (!wpm_archive_extract(stored_archive_path, staging_path)) goto cleanup;
    if (!verify_package_index(staging_path)) goto cleanup;
    if (!run_package_script(staging_path, ".wpm\\remove.cmd", "removal", NULL)) goto cleanup;
    if (!DeleteFileA(stored_archive_path)) {
        printf("Error: could not remove stored package archive: %s\n", stored_archive_path);
        goto cleanup;
    }
    success = 1;

cleanup:
    if (!remove_directory_tree(staging_path)) {
        printf("Error: could not remove staging directory: %s\n", staging_path);
        success = 0;
    }
    if (!success) return 0;

    printf("Removed package: %s\n", archive_name);
    return 1;
}
