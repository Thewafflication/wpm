/** @file archive.c @brief Package archive operations. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <windows.h>

#include "archive.h"
#include "logging.h"
#include "progress.h"
#include "helpers.h"
#include "mz.h"
#include "mz_strm.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"
#include "zlib-ng.h"
#include "sodium.h"
#include "signing.h"

#define WPM_PATH_SIZE 4096
#define WPM_DEFAULT_DATA_ROOT "C:\\ProgramData\\WPM"
#define WPM_MAX_IGNORE_PATTERNS 128
#define WPM_BLAKE2B_BYTES crypto_generichash_BYTES
#define WPM_BLAKE2B_HEX_SIZE (WPM_BLAKE2B_BYTES * 2 + 1)
#define WPM_COMPRESSION_SAMPLE_BYTES (64U * 1024U)
#define WPM_COMPRESSION_SAMPLE_MIN_FILE_BYTES (1024ULL * 1024ULL)
#define WPM_INCOMPRESSIBLE_SAMPLE_PERCENT 98U
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef WPM_ZLIB_NG_BUFFER_BYTES
#define WPM_ZLIB_NG_BUFFER_BYTES (1024U * 1024U)
#endif
#ifndef WPM_ZLIB_NG_COMPRESSION_LEVEL
#define WPM_ZLIB_NG_COMPRESSION_LEVEL 2
#endif

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

typedef struct wpm_process_entry {
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD th32ModuleID;
    DWORD cntThreads;
    DWORD th32ParentProcessID;
    LONG pcPriClassBase;
    DWORD dwFlags;
    CHAR szExeFile[MAX_PATH];
} wpm_process_entry;

typedef HANDLE (WINAPI *wpm_create_process_snapshot_fn)(DWORD, DWORD);
typedef BOOL (WINAPI *wpm_process_first_fn)(HANDLE, wpm_process_entry*);
typedef BOOL (WINAPI *wpm_process_next_fn)(HANDLE, wpm_process_entry*);

typedef struct wpm_script_terminal {
    HANDLE output;
    WORD default_attributes;
    WORD attributes;
    char csi[64];
    size_t csi_length;
    int state;
    int interactive;
    int translate_ansi;
} wpm_script_terminal;

typedef struct wpm_script_log_filter {
    int state;
} wpm_script_log_filter;

static int wpm_verbose = 0;
static int wpm_progress_current = 1;
static int wpm_progress_total = 1;
static char wpm_repository_url[WPM_PATH_SIZE];

void wpm_archive_set_progress(int current, int total) {
    wpm_progress_current = current > 0 ? current : 1;
    wpm_progress_total = total > 0 ? total : 1;
}

static void print_package_progress(const char* package_name, const char* phase) {
    if (wpm_progress_total > 1) {
        printf("[%d of %d] %s: %s...\n", wpm_progress_current, wpm_progress_total, package_name, phase);
    }
    else {
        printf("%s: %s...\n", package_name, phase);
    }
}

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
    char message[8192];

    if (!wpm_verbose) return;
    va_start(arguments, format);
    if (vsnprintf(message, sizeof(message), format, arguments) >= 0) {
        message[sizeof(message) - 1] = '\0';
        printf("Verbose: %s\n", message);
    }
    va_end(arguments);
}

static const char* path_basename(const char* path);
static int normalized_full_path(const char* path, char* result, size_t result_size);
static int join_path(char* result, size_t result_size, const char* left, const char* right);
static const char* active_repository_url(char* inherited, size_t inherited_size);

typedef struct wpm_zlib_ng_buffer {
    unsigned char* data;
    size_t size;
    size_t capacity;
} wpm_zlib_ng_buffer;

typedef struct wpm_zip_output {
    HANDLE file;
    unsigned char* memory;
    uint64_t capacity;
    uint64_t size;
    uint32_t crc32;
    wpm_progress* progress;
} wpm_zip_output;

static int wpm_zlib_ng_buffer_reserve(wpm_zlib_ng_buffer* buffer, size_t additional) {
    size_t maximum = (size_t)-1;
    size_t required;
    size_t capacity;
    unsigned char* resized;

    if (additional > maximum - buffer->size) return 0;
    required = buffer->size + additional;
    if (required <= buffer->capacity) return 1;

    capacity = buffer->capacity ? buffer->capacity : WPM_ZLIB_NG_BUFFER_BYTES;
    while (capacity < required) {
        if (capacity > maximum / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }

    resized = (unsigned char*)realloc(buffer->data, capacity);
    if (!resized) return 0;
    buffer->data = resized;
    buffer->capacity = capacity;
    return 1;
}

void wpm_archive_set_repository_url(const char* url) {
    if (url && strlen(url) < sizeof(wpm_repository_url) &&
        (_strnicmp(url, "https://", 8) == 0 || _strnicmp(url, "http://", 7) == 0) &&
        !strpbrk(url, "\r\n")) {
        strcpy_s(wpm_repository_url, sizeof(wpm_repository_url), url);
    }
    else {
        wpm_repository_url[0] = '\0';
    }
}

static int wpm_zlib_ng_compress_file(
    HANDLE file,
    uint64_t file_size,
    int compression_level,
    wpm_zlib_ng_buffer* compressed,
    uint32_t* crc32
) {
    zng_stream stream;
    unsigned char* input = NULL;
    LARGE_INTEGER beginning;
    uint64_t remaining = file_size;
    int initialized = 0;
    int success = 0;

    memset(&stream, 0, sizeof(stream));
    memset(compressed, 0, sizeof(*compressed));
    *crc32 = 0;

    input = (unsigned char*)malloc(WPM_ZLIB_NG_BUFFER_BYTES);
    if (!input) goto cleanup;

    beginning.QuadPart = 0;
    if (!SetFilePointerEx(file, beginning, NULL, FILE_BEGIN)) goto cleanup;
    if (zng_deflateInit2(
            &stream,
            compression_level,
            Z_DEFLATED,
            -15,
            8,
            Z_DEFAULT_STRATEGY
        ) != Z_OK) {
        goto cleanup;
    }
    initialized = 1;

    while (remaining) {
        DWORD requested = (DWORD)(remaining > WPM_ZLIB_NG_BUFFER_BYTES
            ? WPM_ZLIB_NG_BUFFER_BYTES : remaining);
        DWORD bytes_read = 0;

        if (!ReadFile(file, input, requested, &bytes_read, NULL) || bytes_read != requested) {
            goto cleanup;
        }
        remaining -= bytes_read;
        *crc32 = (uint32_t)zng_crc32(*crc32, input, bytes_read);
        stream.next_in = input;
        stream.avail_in = bytes_read;

        while (stream.avail_in) {
            uint32_t available;
            int status;

            if (!wpm_zlib_ng_buffer_reserve(compressed, WPM_ZLIB_NG_BUFFER_BYTES)) {
                goto cleanup;
            }
            stream.next_out = compressed->data + compressed->size;
            stream.avail_out = WPM_ZLIB_NG_BUFFER_BYTES;
            available = stream.avail_out;
            status = zng_deflate(&stream, Z_NO_FLUSH);
            compressed->size += available - stream.avail_out;
            if (status != Z_OK) goto cleanup;
        }
    }

    for (;;) {
        uint32_t available;
        int status;

        if (!wpm_zlib_ng_buffer_reserve(compressed, WPM_ZLIB_NG_BUFFER_BYTES)) {
            goto cleanup;
        }
        stream.next_out = compressed->data + compressed->size;
        stream.avail_out = WPM_ZLIB_NG_BUFFER_BYTES;
        available = stream.avail_out;
        status = zng_deflate(&stream, Z_FINISH);
        compressed->size += available - stream.avail_out;
        if (status == Z_STREAM_END) break;
        if (status != Z_OK) goto cleanup;
    }

    success = 1;

cleanup:
    if (initialized && zng_deflateEnd(&stream) != Z_OK) success = 0;
    free(input);
    if (!success) {
        free(compressed->data);
        memset(compressed, 0, sizeof(*compressed));
    }
    return success;
}

static int wpm_zlib_ng_sample_is_incompressible(
    const unsigned char* sample,
    size_t sample_size,
    int* result
) {
    size_t compressed_capacity = zng_compressBound(sample_size);
    size_t compressed_size = compressed_capacity;
    unsigned char* compressed = (unsigned char*)malloc(compressed_capacity);
    int status;

    if (!compressed) return 0;
    status = zng_compress2(compressed, &compressed_size, sample, sample_size, Z_BEST_SPEED);
    free(compressed);
    if (status != Z_OK) return 0;

    *result = compressed_size * 100U >=
        sample_size * WPM_INCOMPRESSIBLE_SAMPLE_PERCENT;
    return 1;
}

static int wpm_zip_output_write(wpm_zip_output* output, const unsigned char* data, size_t size) {
    DWORD bytes_written = 0;

    if (output->size > output->capacity ||
        (uint64_t)size > output->capacity - output->size) return 0;
    if (output->file != INVALID_HANDLE_VALUE &&
        (!WriteFile(output->file, data, (DWORD)size, &bytes_written, NULL) ||
         bytes_written != size)) {
        return 0;
    }
    if (output->memory) memcpy(output->memory + output->size, data, size);
    output->crc32 = (uint32_t)zng_crc32(output->crc32, data, (uint32_t)size);
    output->size += size;
    if (output->progress) wpm_progress_add(output->progress, size);
    return 1;
}

static int wpm_zip_read_current_entry(
    void* reader,
    const mz_zip_file* info,
    wpm_zip_output* output
) {
    zng_stream stream;
    unsigned char* input = NULL;
    unsigned char* inflated = NULL;
    uint64_t compressed_read = 0;
    int initialized = 0;
    int opened = 0;
    int finished = info->compression_method == MZ_COMPRESS_METHOD_STORE;
    int success = 0;

    memset(&stream, 0, sizeof(stream));
    if (info->compressed_size < 0 || info->uncompressed_size < 0 ||
        (info->flag & MZ_ZIP_FLAG_ENCRYPTED) != 0 ||
        (info->compression_method != MZ_COMPRESS_METHOD_STORE &&
         info->compression_method != MZ_COMPRESS_METHOD_DEFLATE)) {
        return 0;
    }
    if ((uint64_t)info->uncompressed_size > output->capacity) return 0;

    input = (unsigned char*)malloc(WPM_ZLIB_NG_BUFFER_BYTES);
    if (!input) goto cleanup;
    if (info->compression_method == MZ_COMPRESS_METHOD_DEFLATE) {
        inflated = (unsigned char*)malloc(WPM_ZLIB_NG_BUFFER_BYTES);
        if (!inflated || zng_inflateInit2(&stream, -15) != Z_OK) goto cleanup;
        initialized = 1;
    }
    if (mz_zip_reader_entry_open(reader) != MZ_OK) goto cleanup;
    opened = 1;

    for (;;) {
        int32_t read = mz_zip_reader_entry_read(reader, input, WPM_ZLIB_NG_BUFFER_BYTES);
        if (read < 0) goto cleanup;
        if (read == 0) break;
        compressed_read += (uint32_t)read;
        if (compressed_read > (uint64_t)info->compressed_size) goto cleanup;

        if (info->compression_method == MZ_COMPRESS_METHOD_STORE) {
            if (!wpm_zip_output_write(output, input, (size_t)read)) goto cleanup;
            continue;
        }

        stream.next_in = input;
        stream.avail_in = (uint32_t)read;
        while (stream.avail_in) {
            uint32_t input_before = stream.avail_in;
            uint32_t available = WPM_ZLIB_NG_BUFFER_BYTES;
            size_t produced;
            int status;

            stream.next_out = inflated;
            stream.avail_out = available;
            status = zng_inflate(&stream, Z_NO_FLUSH);
            produced = available - stream.avail_out;
            if (produced && !wpm_zip_output_write(output, inflated, produced)) goto cleanup;
            if (status == Z_STREAM_END) {
                if (stream.avail_in) goto cleanup;
                finished = 1;
                break;
            }
            if (status != Z_OK ||
                (input_before == stream.avail_in && produced == 0)) goto cleanup;
        }
    }

    if (mz_zip_reader_entry_close(reader) != MZ_OK) goto cleanup;
    opened = 0;
    success = finished && compressed_read == (uint64_t)info->compressed_size &&
        output->size == (uint64_t)info->uncompressed_size && output->crc32 == info->crc;

cleanup:
    if (opened) mz_zip_reader_entry_close(reader);
    if (initialized && zng_inflateEnd(&stream) != Z_OK) success = 0;
    free(inflated);
    free(input);
    return success;
}

static void wpm_set_file_modified_time(HANDLE file, time_t modified) {
    const ULONGLONG windows_epoch_seconds = 11644473600ULL;
    ULONGLONG seconds;
    ULONGLONG ticks;
    FILETIME write_time;

    if ((LONGLONG)modified < 0) return;
    seconds = (ULONGLONG)modified;
    if (seconds > ((ULONGLONG)-1) / 10000000ULL - windows_epoch_seconds) return;
    ticks = (seconds + windows_epoch_seconds) * 10000000ULL;
    write_time.dwLowDateTime = (DWORD)ticks;
    write_time.dwHighDateTime = (DWORD)(ticks >> 32);
    SetFileTime(file, NULL, NULL, &write_time);
}

static int wpm_zip_extract_current_file(
    void* reader,
    const mz_zip_file* info,
    const char* destination_path,
    wpm_progress* progress
) {
    wpm_zip_output output;
    int success = 0;

    memset(&output, 0, sizeof(output));
    output.file = CreateFileA(
        destination_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (output.file == INVALID_HANDLE_VALUE) goto cleanup;
    output.capacity = (uint64_t)info->uncompressed_size;
    output.progress = progress;
    if (!wpm_zip_read_current_entry(reader, info, &output)) goto cleanup;
    wpm_set_file_modified_time(output.file, info->modified_date);
    success = 1;

cleanup:
    if (output.file != INVALID_HANDLE_VALUE && !CloseHandle(output.file)) success = 0;
    if (!success) DeleteFileA(destination_path);
    return success;
}

static int wpm_zip_read_current_memory(
    void* reader,
    const mz_zip_file* info,
    unsigned char* memory,
    size_t capacity
) {
    wpm_zip_output output;

    memset(&output, 0, sizeof(output));
    output.file = INVALID_HANDLE_VALUE;
    output.memory = memory;
    output.capacity = capacity;
    return wpm_zip_read_current_entry(reader, info, &output);
}

static int wpm_zip_should_store(HANDLE file, uint64_t file_size) {
    unsigned char* sample;
    DWORD bytes_read = 0;
    LARGE_INTEGER beginning;
    int is_incompressible;

    if (file_size < WPM_COMPRESSION_SAMPLE_MIN_FILE_BYTES) return 0;

    sample = (unsigned char*)malloc(WPM_COMPRESSION_SAMPLE_BYTES);
    if (!sample) return 0;
    beginning.QuadPart = 0;
    if (!SetFilePointerEx(file, beginning, NULL, FILE_BEGIN) ||
        !ReadFile(
            file,
            sample,
            WPM_COMPRESSION_SAMPLE_BYTES,
            &bytes_read,
            NULL
        ) || !bytes_read) {
        free(sample);
        return 0;
    }

    if (!wpm_zlib_ng_sample_is_incompressible(sample, bytes_read, &is_incompressible)) {
        free(sample);
        return 0;
    }
    free(sample);
    return is_incompressible;
}

static time_t wpm_get_file_modified_time(HANDLE file) {
    const ULONGLONG windows_epoch_seconds = 11644473600ULL;
    FILETIME modified;
    ULARGE_INTEGER ticks;

    if (!GetFileTime(file, NULL, NULL, &modified)) return 0;
    ticks.LowPart = modified.dwLowDateTime;
    ticks.HighPart = modified.dwHighDateTime;
    if (ticks.QuadPart / 10000000ULL < windows_epoch_seconds) return 0;
    return (time_t)(ticks.QuadPart / 10000000ULL - windows_epoch_seconds);
}

static void wpm_zip_initialize_file_info(
    mz_zip_file* info,
    const char* archive_path,
    uint16_t compression_method,
    uint64_t uncompressed_size,
    uint64_t compressed_size,
    uint32_t crc32,
    time_t modified_date,
    DWORD attributes
) {
    memset(info, 0, sizeof(*info));
    info->version_madeby = (uint16_t)(MZ_HOST_SYSTEM_WINDOWS_NTFS << 8);
    info->flag = MZ_ZIP_FLAG_UTF8;
    info->compression_method = compression_method;
    info->modified_date = modified_date;
    info->crc = crc32;
    info->compressed_size = (int64_t)compressed_size;
    info->uncompressed_size = (int64_t)uncompressed_size;
    info->external_fa = attributes;
    info->filename = archive_path;
    info->zip64 = MZ_ZIP64_AUTO;
}

static int wpm_zip_writer_write_all(void* writer, const unsigned char* data, size_t size) {
    while (size) {
        int32_t chunk = (int32_t)(size > WPM_ZLIB_NG_BUFFER_BYTES
            ? WPM_ZLIB_NG_BUFFER_BYTES : size);
        if (mz_zip_writer_entry_write(writer, data, chunk) != chunk) return 0;
        data += chunk;
        size -= (size_t)chunk;
    }
    return 1;
}

static int wpm_zip_writer_close_raw(void* writer, uint64_t uncompressed_size, uint32_t crc32) {
    void* zip_handle = NULL;

    if (uncompressed_size > 0x7FFFFFFFFFFFFFFFULL ||
        mz_zip_writer_get_zip_handle(writer, &zip_handle) != MZ_OK) {
        return 0;
    }
    return mz_zip_entry_close_raw(zip_handle, (int64_t)uncompressed_size, crc32) == MZ_OK;
}

static int wpm_zip_add_stored_file(
    void* writer,
    HANDLE file,
    const char* archive_path,
    uint64_t file_size,
    time_t modified_date,
    DWORD attributes
) {
    mz_zip_file info;
    unsigned char* buffer = NULL;
    LARGE_INTEGER beginning;
    uint64_t remaining = file_size;
    uint32_t crc32 = 0;
    int opened = 0;
    int success = 0;

    wpm_zip_initialize_file_info(&info, archive_path, MZ_COMPRESS_METHOD_STORE,
        file_size, file_size, 0, modified_date, attributes);
    if (mz_zip_writer_entry_open(writer, &info) != MZ_OK) goto cleanup;
    opened = 1;
    buffer = (unsigned char*)malloc(WPM_ZLIB_NG_BUFFER_BYTES);
    if (!buffer) goto cleanup;
    beginning.QuadPart = 0;
    if (!SetFilePointerEx(file, beginning, NULL, FILE_BEGIN)) goto cleanup;

    while (remaining) {
        DWORD requested = (DWORD)(remaining > WPM_ZLIB_NG_BUFFER_BYTES
            ? WPM_ZLIB_NG_BUFFER_BYTES : remaining);
        DWORD bytes_read = 0;
        if (!ReadFile(file, buffer, requested, &bytes_read, NULL) || bytes_read != requested ||
            mz_zip_writer_entry_write(writer, buffer, (int32_t)bytes_read) != (int32_t)bytes_read) {
            goto cleanup;
        }
        crc32 = (uint32_t)zng_crc32(crc32, buffer, bytes_read);
        remaining -= bytes_read;
    }

    success = wpm_zip_writer_close_raw(writer, file_size, crc32);
    opened = 0;

cleanup:
    free(buffer);
    if (opened) wpm_zip_writer_close_raw(writer, file_size, crc32);
    return success;
}

static int wpm_zip_add_file(void* writer, const char* archive_path, const char* source_path) {
    HANDLE file;
    LARGE_INTEGER size;
    DWORD size_high = 0;
    DWORD size_low;
    DWORD attributes;
    time_t modified_date;
    int success = 0;

    file = CreateFileA(source_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    SetLastError(NO_ERROR);
    size_low = GetFileSize(file, &size_high);
    if (size_low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) goto cleanup;
    size.HighPart = (LONG)size_high;
    size.LowPart = size_low;
    attributes = GetFileAttributesA(source_path);
    if (attributes == INVALID_FILE_ATTRIBUTES) attributes = FILE_ATTRIBUTE_NORMAL;
    modified_date = wpm_get_file_modified_time(file);

    if (!wpm_zip_should_store(file, (uint64_t)size.QuadPart)) {
        wpm_zlib_ng_buffer compressed;
        mz_zip_file info;
        uint32_t crc32;

        if (wpm_zlib_ng_compress_file(
                file,
                (uint64_t)size.QuadPart,
                WPM_ZLIB_NG_COMPRESSION_LEVEL,
                &compressed,
                &crc32
            )) {
            verbose_log("Compressing with zlib-ng: %s", archive_path);
            wpm_zip_initialize_file_info(&info, archive_path, MZ_COMPRESS_METHOD_DEFLATE,
                (uint64_t)size.QuadPart, compressed.size, crc32, modified_date, attributes);
            if (mz_zip_writer_entry_open(writer, &info) == MZ_OK &&
                wpm_zip_writer_write_all(writer, compressed.data, compressed.size) &&
                wpm_zip_writer_close_raw(writer, (uint64_t)size.QuadPart, crc32)) {
                success = 1;
            }
            free(compressed.data);
            goto cleanup;
        }
        verbose_log("zlib-ng compression unavailable; storing without compression: %s", archive_path);
    }

    success = wpm_zip_add_stored_file(writer, file, archive_path,
        (uint64_t)size.QuadPart, modified_date, attributes);

cleanup:
    CloseHandle(file);
    return success;
}

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

static int read_archive_package_metadata(const char* archive_path, wpm_package_metadata* metadata) {
    void* reader = NULL;
    mz_zip_file* info = NULL;
    char* text = NULL;
    char* current;
    int result = 0;

    reader = mz_zip_reader_create();
    if (!reader || mz_zip_reader_open_file(reader, archive_path) != MZ_OK) goto cleanup;
    mz_zip_reader_set_raw(reader, 1);
    if (mz_zip_reader_locate_entry(reader, ".wpm/package.txt", 0) != MZ_OK ||
        mz_zip_reader_entry_get_info(reader, &info) != MZ_OK ||
        info->uncompressed_size < 0 || info->uncompressed_size > 1024 * 1024) goto cleanup;

    text = malloc((size_t)info->uncompressed_size + 1);
    if (!text || !wpm_zip_read_current_memory(reader, info, (unsigned char*)text,
        (size_t)info->uncompressed_size)) goto cleanup;
    text[(size_t)info->uncompressed_size] = '\0';

    metadata->name[0] = '\0';
    metadata->version[0] = '\0';
    metadata->arch[0] = '\0';
    metadata->debug = 0;
    current = text;
    while (*current) {
        char* line = current;
        char* end = strpbrk(current, "\r\n");
        char* equals;
        char* key;
        char* value;

        if (end) {
            *end = '\0';
            current = end + 1;
            while (*current == '\r' || *current == '\n') current++;
        }
        else current += strlen(current);

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
            if (strlen(value) >= sizeof(metadata->name)) goto cleanup;
            strcpy_s(metadata->name, sizeof(metadata->name), value);
        }
        else if (_stricmp(key, "version") == 0) {
            if (strlen(value) >= sizeof(metadata->version)) goto cleanup;
            strcpy_s(metadata->version, sizeof(metadata->version), value);
        }
        else if (_stricmp(key, "arch") == 0) {
            if (strlen(value) >= sizeof(metadata->arch)) goto cleanup;
            strcpy_s(metadata->arch, sizeof(metadata->arch), value);
        }
        else if (_stricmp(key, "debug") == 0 &&
            !parse_bool_metadata_value(value, &metadata->debug)) goto cleanup;
    }

    result = is_safe_metadata_value(metadata->name) &&
        is_safe_metadata_value(metadata->version) &&
        is_safe_metadata_value(metadata->arch);

cleanup:
    free(text);
    mz_zip_reader_delete(&reader);
    return result;
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

static int repository_record_path(const char* archive_path, char* path, size_t path_size) {
    int written = snprintf(path, path_size, "%s.repository.txt", archive_path);
    return written >= 0 && (size_t)written < path_size;
}

static int record_archive_repository(const char* archive_path) {
    char path[WPM_PATH_SIZE], inherited[WPM_PATH_SIZE];
    const char* repository_url = active_repository_url(inherited, sizeof(inherited));
    FILE* file;
    if (!repository_record_path(archive_path, path, sizeof(path))) return 0;
    if (!repository_url) {
        DeleteFileA(path);
        return 1;
    }
    file = wpm_fopen(path, "wb");
    if (!file) return 0;
    fprintf(file, "%s\n", repository_url);
    return fclose(file) == 0;
}

static int load_archive_repository(const char* archive_path, char* result, size_t result_size) {
    char path[WPM_PATH_SIZE];
    FILE* file;
    if (!repository_record_path(archive_path, path, sizeof(path)) ||
        (file = wpm_fopen(path, "r")) == NULL) return 0;
    if (!fgets(result, (int)result_size, file)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    result[strcspn(result, "\r\n")] = '\0';
    return (_strnicmp(result, "https://", 8) == 0 ||
            _strnicmp(result, "http://", 7) == 0) && !strpbrk(result, "\r\n");
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

static int calculate_file_blake2b(
    const char* path,
    char* hex,
    size_t hex_size,
    wpm_progress* progress
) {
    unsigned char buffer[8192];
    unsigned char hash[WPM_BLAKE2B_BYTES];
    crypto_generichash_state state;
    FILE* file;

    verbose_log("Computing BLAKE2b hash: %s", path);
    file = wpm_fopen(path, "rb");

    if (!file) {
        verbose_log("Could not open hash input");
        return 0;
    }
    if (!ensure_sodium_ready() ||
        crypto_generichash_init(&state, NULL, 0, sizeof(hash)) != 0) {
        verbose_log("Could not initialize BLAKE2b state");
        fclose(file);
        return 0;
    }

    for (;;) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
        if (bytes_read > 0) {
            if (crypto_generichash_update(&state, buffer, bytes_read) != 0) {
                verbose_log("Could not update BLAKE2b state");
                fclose(file);
                return 0;
            }
            if (progress) wpm_progress_add(progress, bytes_read);
        }
        if (bytes_read < sizeof(buffer)) {
            if (ferror(file)) {
                verbose_log("Hash input reported a read error after %lu bytes", (unsigned long)bytes_read);
                fclose(file);
                return 0;
            }
            break;
        }
    }

    if (crypto_generichash_final(&state, hash, sizeof(hash)) != 0) {
        verbose_log("Could not finalize BLAKE2b state");
        fclose(file);
        return 0;
    }

    sodium_bin2hex(hex, hex_size, hash, sizeof(hash));
    if (fclose(file) != 0) {
        verbose_log("Could not close hash input");
        return 0;
    }
    return 1;
}

static int remove_directory_tree_with_retry(const char* path) {
    unsigned int attempt;

    for (attempt = 0; attempt < 100; attempt++) {
        if (remove_directory_tree(path)) return 1;
        Sleep(50);
    }
    return remove_directory_tree(path);
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
            char index_line[WPM_PATH_SIZE + WPM_BLAKE2B_HEX_SIZE + 64];
            unsigned long long file_size;

            if (!normalized_full_path(source_path, source_full_path, sizeof(source_full_path))) {
                FindClose(search);
                return 0;
            }
            if (_stricmp(source_full_path, output_archive) == 0) continue;

            verbose_log("Indexing file: %s", archive_path);

            if (!get_file_size_bytes(source_path, &file_size) ||
                !calculate_file_blake2b(source_path, blake2b, sizeof(blake2b), NULL) ||
                snprintf(index_line, sizeof(index_line), "%s,%llu,%s,blake2b\n",
                         archive_path, file_size, blake2b) < 0 ||
                fputs(index_line, index) < 0) {
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
    void* writer,
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
            mz_zip_file info;
            int written = snprintf(directory_entry, sizeof(directory_entry), "%s/", archive_path);
            if (written < 0 || (size_t)written >= sizeof(directory_entry)) {
                FindClose(search);
                return 0;
            }
            wpm_zip_initialize_file_info(&info, directory_entry, MZ_COMPRESS_METHOD_STORE,
                0, 0, 0, 0, entry.dwFileAttributes);
            if (mz_zip_writer_entry_open(writer, &info) != MZ_OK ||
                !wpm_zip_writer_close_raw(writer, 0, 0) ||
                !add_directory_to_zip(writer, source_path, archive_path, output_archive)) {
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

            if (!wpm_zip_add_file(writer, archive_path, source_path)) {
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
    void* writer = NULL;
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

    writer = mz_zip_writer_create();
    if (!writer) {
        remove(output_path);
        printf("Error: could not create archive: %s\n", output_path);
        return 0;
    }
    mz_zip_writer_set_raw(writer, 1);
    if (mz_zip_writer_open_file(writer, output_path, 0, 0) != MZ_OK) goto writer_cleanup;

    if (add_directory_to_zip(writer, source_full_path, "", output_path) &&
        mz_zip_writer_close(writer) == MZ_OK) {
        success = 1;
    }

writer_cleanup:
    mz_zip_writer_delete(&writer);

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

static int create_parent_directory(const char* path, char* previous_parent, size_t previous_parent_size) {
    char parent[WPM_PATH_SIZE];
    char* separator;

    strcpy_s(parent, sizeof(parent), path);
    separator = strrchr(parent, '\\');
    if (!separator) return 1;
    *separator = '\0';
    if (_stricmp(parent, previous_parent) == 0) return 1;
    if (!create_directories(parent)) return 0;
    return strcpy_s(previous_parent, previous_parent_size, parent) == 0;
}

static int measure_archive_uncompressed_bytes(void* reader, uint64_t* total) {
    int32_t entry_status = mz_zip_reader_goto_first_entry(reader);
    *total = 0;
    while (entry_status == MZ_OK) {
        mz_zip_file* info = NULL;
        if (mz_zip_reader_entry_get_info(reader, &info) != MZ_OK || !info ||
            info->uncompressed_size < 0) return 0;
        if (mz_zip_reader_entry_is_dir(reader) != MZ_OK) {
            uint64_t size = (uint64_t)info->uncompressed_size;
            if (*total > ULLONG_MAX - size) return 0;
            *total += size;
        }
        entry_status = mz_zip_reader_goto_next_entry(reader);
    }
    return entry_status == MZ_END_OF_LIST;
}

static int wpm_archive_extract_with_label(
    const char* archive_path,
    const char* destination_dir,
    const char* progress_label
) {
    void* reader = NULL;
    char previous_parent[WPM_PATH_SIZE] = "";
    int32_t entry_status;
    uint64_t total_bytes = 0;
    wpm_progress progress;
    int progress_started = 0;
    int success = 0;

    verbose_log("Extracting archive: %s", archive_path);

    if (!create_directories(destination_dir)) {
        printf("Error: could not create extraction directory: %s\n", destination_dir);
        return 0;
    }

    reader = mz_zip_reader_create();
    if (!reader || mz_zip_reader_open_file(reader, archive_path) != MZ_OK) {
        printf("Error: could not open package archive: %s\n", archive_path);
        goto cleanup;
    }
    mz_zip_reader_set_raw(reader, 1);
    if (!measure_archive_uncompressed_bytes(reader, &total_bytes)) total_bytes = 0;
    wpm_progress_start(&progress, "Extracting", "Extraction", "Extracted",
        progress_label, total_bytes);
    progress_started = 1;

    entry_status = mz_zip_reader_goto_first_entry(reader);
    while (entry_status == MZ_OK) {
        mz_zip_file* info = NULL;
        char relative_path[WPM_PATH_SIZE];
        char destination_path[WPM_PATH_SIZE];

        if (mz_zip_reader_entry_get_info(reader, &info) != MZ_OK || !info ||
            !info->filename || strlen(info->filename) >= sizeof(relative_path) ||
            !is_safe_archive_path(info->filename) ||
            mz_zip_attrib_is_symlink(info->external_fa, info->version_madeby) == MZ_OK) {
            printf("Error: package contains an invalid path.\n");
            goto cleanup;
        }

        strcpy_s(relative_path, sizeof(relative_path), info->filename);
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

        if (mz_zip_reader_entry_is_dir(reader) == MZ_OK) {
            verbose_log("Creating directory: %s", destination_path);
            if (!create_directories(destination_path)) {
                printf("Error: could not create directory: %s\n", destination_path);
                goto cleanup;
            }
        }
        else {
            verbose_log("Extracting file: %s", destination_path);
            if (!create_parent_directory(destination_path, previous_parent, sizeof(previous_parent))) {
                printf("Error: could not extract file: %s\n", destination_path);
                goto cleanup;
            }
            if (info->compression_method == MZ_COMPRESS_METHOD_DEFLATE) {
                verbose_log("Decompressing with zlib-ng: %s", destination_path);
            }
            if (!wpm_zip_extract_current_file(reader, info, destination_path, &progress)) {
                printf("Error: could not extract file: %s\n", destination_path);
                goto cleanup;
            }
        }

        entry_status = mz_zip_reader_goto_next_entry(reader);
    }

    success = entry_status == MZ_END_OF_LIST;

cleanup:
    if (progress_started) wpm_progress_finish(&progress, success);
    mz_zip_reader_delete(&reader);
    return success;
}

int wpm_archive_extract(const char* archive_path, const char* destination_dir) {
    return wpm_archive_extract_with_label(
        archive_path, destination_dir, path_basename(archive_path));
}

static int file_exists_at_path(const char* path) {
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

typedef struct wpm_index_paths {
    char** items;
    size_t count;
} wpm_index_paths;

static int compare_index_paths(const void* left, const void* right) {
    return _stricmp(*(const char* const*)left, *(const char* const*)right);
}

static void free_index_paths(wpm_index_paths* paths) {
    for (size_t i = 0; i < paths->count; i++) free(paths->items[i]);
    free(paths->items);
    paths->items = NULL;
    paths->count = 0;
}

static int load_index_paths(const char* index_path, wpm_index_paths* paths) {
    char line[WPM_PATH_SIZE + WPM_BLAKE2B_HEX_SIZE + 64];
    FILE* index = wpm_fopen(index_path, "r");
    memset(paths, 0, sizeof(*paths));
    if (!index) return 0;

    while (fgets(line, sizeof(line), index)) {
        char* comma;
        char* item;
        char** resized;
        trim_line(line);
        if (_stricmp(line, "filename,size,hash,algorithm") == 0) continue;
        comma = strchr(line, ',');
        if (!comma) continue;
        *comma = '\0';
        normalize_archive_separators(line);
        item = (char*)malloc(strlen(line) + 1);
        if (!item) {
            fclose(index);
            free_index_paths(paths);
            return 0;
        }
        strcpy_s(item, strlen(line) + 1, line);
        resized = (char**)realloc(paths->items, (paths->count + 1) * sizeof(*paths->items));
        if (!resized) {
            free(item);
            fclose(index);
            free_index_paths(paths);
            return 0;
        }
        paths->items = resized;
        paths->items[paths->count++] = item;
    }
    if (ferror(index)) {
        fclose(index);
        free_index_paths(paths);
        return 0;
    }
    fclose(index);
    if (paths->count > 1) {
        qsort(paths->items, paths->count, sizeof(*paths->items), compare_index_paths);
    }
    return 1;
}

static int index_contains_path(const wpm_index_paths* paths, const char* archive_path) {
    const char* key = archive_path;
    if (paths->count == 0) return 0;
    return bsearch(&key, paths->items, paths->count, sizeof(*paths->items), compare_index_paths) != NULL;
}

static int verify_index_completeness(const char* destination_dir, const char* relative_dir, const wpm_index_paths* paths) {
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
            if (!verify_index_completeness(destination_dir, relative_path, paths)) {
                FindClose(search);
                return 0;
            }
        } else if (_stricmp(relative_path, ".wpm/index.csv") != 0 &&
                   _stricmp(relative_path, ".wpm/signature.json") != 0 &&
                   !index_contains_path(paths, relative_path)) {
            printf("Error: package contains unindexed file: %s.\n", relative_path);
            FindClose(search);
            return 0;
        }
    } while (FindNextFileA(search, &entry));

    FindClose(search);
    return 1;
}

static int measure_package_index_bytes(
    const char* index_path,
    unsigned long long* total_bytes
) {
    char line[WPM_PATH_SIZE + WPM_BLAKE2B_HEX_SIZE + 64];
    FILE* index = wpm_fopen(index_path, "r");
    unsigned long line_number = 0;
    int valid = 1;

    *total_bytes = 0;
    if (!index) return 0;
    while (fgets(line, sizeof(line), index)) {
        char* comma;
        char* second_comma;
        char* size_text;
        unsigned long long size;
        char trailing;
        line_number++;
        trim_line(line);
        if (line_number == 1 &&
            _stricmp(line, "filename,size,hash,algorithm") == 0) continue;
        if (!line[0]) continue;
        comma = strchr(line, ',');
        second_comma = comma ? strchr(comma + 1, ',') : NULL;
        if (!comma || !second_comma) {
            valid = 0;
            break;
        }
        *second_comma = '\0';
        size_text = comma + 1;
        trim_line(size_text);
        if (sscanf_s(size_text, "%llu%c", &size, &trailing, 1) != 1 ||
            *total_bytes > ULLONG_MAX - size) {
            valid = 0;
            break;
        }
        *total_bytes += size;
    }
    if (ferror(index)) valid = 0;
    if (fclose(index) != 0) valid = 0;
    if (!valid) *total_bytes = 0;
    return valid;
}

static int verify_package_index_contents(
    const char* destination_dir,
    const char* index_path,
    wpm_progress* progress
) {
    char line[WPM_PATH_SIZE + WPM_BLAKE2B_HEX_SIZE + 64];
    FILE* index;
    unsigned long line_number = 0;

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
            !calculate_file_blake2b(file_path, actual_hash, sizeof(actual_hash), progress) ||
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
        wpm_index_paths paths;
        int complete;
        if (!join_path(signature_path, sizeof(signature_path), destination_dir, ".wpm\\signature.json")) return 0;
        if (!file_exists_at_path(signature_path)) return 1;
        if (!load_index_paths(index_path, &paths)) {
            printf("Error: could not load package index paths.\n");
            return 0;
        }
        complete = verify_index_completeness(destination_dir, "", &paths);
        free_index_paths(&paths);
        return complete;
    }
}

static int verify_package_index(const char* destination_dir, const char* progress_label) {
    char index_path[WPM_PATH_SIZE];
    unsigned long long total_bytes = 0;
    wpm_progress progress;
    int result;

    if (!join_path(index_path, sizeof(index_path), destination_dir, ".wpm\\index.csv")) {
        printf("Error: package index path is too long.\n");
        return 0;
    }
    if (!file_exists_at_path(index_path)) return 1;
    measure_package_index_bytes(index_path, &total_bytes);
    wpm_progress_start(&progress, "Validating", "Validation", "Validated",
        progress_label, total_bytes);
    result = verify_package_index_contents(destination_dir, index_path, &progress);
    wpm_progress_finish(&progress, result);
    return result;
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

static void report_script_children(DWORD script_pid) {
    HANDLE snapshot;
    HMODULE kernel32;
    wpm_create_process_snapshot_fn create_snapshot;
    wpm_process_first_fn process_first;
    wpm_process_next_fn process_next;
    wpm_process_entry process;
    int found = 0;

    kernel32 = GetModuleHandleA("kernel32.dll");
    create_snapshot = kernel32 ? (wpm_create_process_snapshot_fn)GetProcAddress(kernel32,
        "CreateToolhelp32Snapshot") : NULL;
    process_first = kernel32 ? (wpm_process_first_fn)GetProcAddress(kernel32,
        "Process32First") : NULL;
    process_next = kernel32 ? (wpm_process_next_fn)GetProcAddress(kernel32,
        "Process32Next") : NULL;
    if (!create_snapshot || !process_first || !process_next) {
        verbose_log("Process inspection APIs are unavailable");
        return;
    }
    snapshot = create_snapshot(0x00000002, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        verbose_log("Could not inspect child processes of script PID %lu (Windows error %lu)",
            (unsigned long)script_pid, (unsigned long)GetLastError());
        return;
    }
    memset(&process, 0, sizeof(process));
    process.dwSize = sizeof(process);
    if (process_first(snapshot, &process)) {
        do {
            if (process.th32ParentProcessID == script_pid) {
                verbose_log("Script child process: PID %lu, executable %s",
                    (unsigned long)process.th32ProcessID, process.szExeFile);
                found = 1;
            }
        } while (process_next(snapshot, &process));
    }
    if (!found) verbose_log("Script PID %lu has no visible direct child process",
        (unsigned long)script_pid);
    CloseHandle(snapshot);
}

static const char* active_repository_url(char* inherited, size_t inherited_size) {
    DWORD length;
    if (wpm_repository_url[0]) return wpm_repository_url;
    length = GetEnvironmentVariableA("WPM_PACKAGE_REPOSITORY_URL", inherited,
        (DWORD)inherited_size);
    if (length == 0 || length >= inherited_size || strpbrk(inherited, "\r\n") ||
        (_strnicmp(inherited, "https://", 8) != 0 &&
         _strnicmp(inherited, "http://", 7) != 0)) return NULL;
    return inherited;
}

static void safe_log_component(char* result, size_t result_size, const char* value) {
    size_t written = 0;
    while (*value && written + 1 < result_size) {
        unsigned char character = (unsigned char)*value++;
        result[written++] = isalnum(character) || character == '-' || character == '_'
            ? (char)character : '-';
    }
    result[written] = '\0';
}

static int create_script_log(const char* package_name, const char* action_name,
                             char* log_path, size_t log_path_size, FILE** log) {
    char data_root[WPM_PATH_SIZE], logs[WPM_PATH_SIZE], scripts[WPM_PATH_SIZE];
    char safe_package[128], safe_action[64];
    SYSTEMTIME now;
    int written;
    if (!wpm_get_data_root(data_root, sizeof(data_root)) ||
        !join_path(logs, sizeof(logs), data_root, "logs") ||
        !join_path(scripts, sizeof(scripts), logs, "scripts") ||
        !create_directories(scripts)) return 0;
    safe_log_component(safe_package, sizeof(safe_package), package_name);
    safe_log_component(safe_action, sizeof(safe_action), action_name);
    GetSystemTime(&now);
    written = snprintf(log_path, log_path_size,
        "%s\\%04u%02u%02uT%02u%02u%02u.%03uZ-%lu-%llu-%s-%s.log",
        scripts, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
        now.wSecond, now.wMilliseconds, (unsigned long)GetCurrentProcessId(),
        (unsigned long long)wpm_tick_count(), safe_package, safe_action);
    if (written < 0 || (size_t)written >= log_path_size) return 0;
    *log = wpm_fopen(log_path, "wb");
    return *log != NULL;
}

enum {
    WPM_ANSI_TEXT,
    WPM_ANSI_ESCAPE,
    WPM_ANSI_CSI,
    WPM_ANSI_OSC,
    WPM_ANSI_OSC_ESCAPE
};

static WORD ansi_color_bits(int color) {
    static const WORD colors[8] = {
        0,
        FOREGROUND_RED,
        FOREGROUND_GREEN,
        FOREGROUND_RED | FOREGROUND_GREEN,
        FOREGROUND_BLUE,
        FOREGROUND_RED | FOREGROUND_BLUE,
        FOREGROUND_GREEN | FOREGROUND_BLUE,
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
    };
    return colors[color & 7];
}

static void apply_script_sgr(wpm_script_terminal* terminal) {
    int parameters[16];
    int count = 0;
    int value = 0;
    int have_value = 0;
    size_t index;
    for (index = 0; index <= terminal->csi_length && count < 16; index++) {
        char character = index < terminal->csi_length ? terminal->csi[index] : ';';
        if (character >= '0' && character <= '9') {
            value = value * 10 + character - '0';
            have_value = 1;
        }
        else if (character == ';') {
            parameters[count++] = have_value ? value : 0;
            value = 0;
            have_value = 0;
        }
    }
    if (count == 0) parameters[count++] = 0;
    for (int parameter = 0; parameter < count; parameter++) {
        int code = parameters[parameter];
        if (code == 0) terminal->attributes = terminal->default_attributes;
        else if (code == 1) terminal->attributes |= FOREGROUND_INTENSITY;
        else if (code == 22) terminal->attributes &= (WORD)~FOREGROUND_INTENSITY;
        else if (code >= 30 && code <= 37) {
            terminal->attributes = (terminal->attributes & (WORD)~0x000F) |
                ansi_color_bits(code - 30);
        }
        else if (code == 39) {
            terminal->attributes = (terminal->attributes & (WORD)~0x000F) |
                (terminal->default_attributes & 0x000F);
        }
        else if (code >= 40 && code <= 47) {
            terminal->attributes = (terminal->attributes & (WORD)~0x00F0) |
                (WORD)(ansi_color_bits(code - 40) << 4);
        }
        else if (code == 49) {
            terminal->attributes = (terminal->attributes & (WORD)~0x00F0) |
                (terminal->default_attributes & 0x00F0);
        }
        else if (code >= 90 && code <= 97) {
            terminal->attributes = (terminal->attributes & (WORD)~0x000F) |
                ansi_color_bits(code - 90) | FOREGROUND_INTENSITY;
        }
        else if (code >= 100 && code <= 107) {
            terminal->attributes = (terminal->attributes & (WORD)~0x00F0) |
                (WORD)((ansi_color_bits(code - 100) | FOREGROUND_INTENSITY) << 4);
        }
        else if ((code == 38 || code == 48) && parameter + 1 < count) {
            if (parameters[parameter + 1] == 5 && parameter + 2 < count) parameter += 2;
            else if (parameters[parameter + 1] == 2 && parameter + 4 < count) parameter += 4;
        }
    }
    SetConsoleTextAttribute(terminal->output, terminal->attributes);
}

static void initialize_script_terminal(wpm_script_terminal* terminal) {
    CONSOLE_SCREEN_BUFFER_INFO information;
    DWORD mode;
    memset(terminal, 0, sizeof(*terminal));
    terminal->output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (terminal->output == NULL || terminal->output == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(terminal->output, &mode) ||
        !GetConsoleScreenBufferInfo(terminal->output, &information)) return;
    terminal->interactive = 1;
    terminal->default_attributes = information.wAttributes;
    terminal->attributes = information.wAttributes;
    if (!SetConsoleMode(terminal->output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        terminal->translate_ansi = 1;
    }
}

static void write_script_terminal(const unsigned char* bytes, DWORD count,
                                  wpm_script_terminal* terminal) {
    DWORD index;
    if (!terminal->translate_ansi) {
        fwrite(bytes, 1, count, stdout);
        fflush(stdout);
        return;
    }
    for (index = 0; index < count; index++) {
        unsigned char character = bytes[index];
        if (terminal->state == WPM_ANSI_TEXT) {
            if (character == 0x1b) terminal->state = WPM_ANSI_ESCAPE;
            else putchar(character);
        }
        else if (terminal->state == WPM_ANSI_ESCAPE) {
            terminal->csi_length = 0;
            if (character == '[') terminal->state = WPM_ANSI_CSI;
            else if (character == ']') terminal->state = WPM_ANSI_OSC;
            else terminal->state = WPM_ANSI_TEXT;
        }
        else if (terminal->state == WPM_ANSI_CSI) {
            if (character >= 0x40 && character <= 0x7e) {
                if (character == 'm') apply_script_sgr(terminal);
                terminal->state = WPM_ANSI_TEXT;
            }
            else if (terminal->csi_length + 1 < sizeof(terminal->csi)) {
                terminal->csi[terminal->csi_length++] = (char)character;
            }
        }
        else if (terminal->state == WPM_ANSI_OSC) {
            if (character == 0x07) terminal->state = WPM_ANSI_TEXT;
            else if (character == 0x1b) terminal->state = WPM_ANSI_OSC_ESCAPE;
        }
        else if (terminal->state == WPM_ANSI_OSC_ESCAPE) {
            terminal->state = character == '\\' ? WPM_ANSI_TEXT : WPM_ANSI_OSC;
        }
    }
    fflush(stdout);
}

static DWORD filter_script_log(const unsigned char* bytes, DWORD count,
                               unsigned char* filtered, wpm_script_log_filter* filter) {
    DWORD index;
    DWORD written = 0;
    for (index = 0; index < count; index++) {
        unsigned char character = bytes[index];
        if (filter->state == WPM_ANSI_TEXT) {
            if (character == 0x1b) filter->state = WPM_ANSI_ESCAPE;
            else filtered[written++] = character;
        }
        else if (filter->state == WPM_ANSI_ESCAPE) {
            if (character == '[') filter->state = WPM_ANSI_CSI;
            else if (character == ']') filter->state = WPM_ANSI_OSC;
            else filter->state = WPM_ANSI_TEXT;
        }
        else if (filter->state == WPM_ANSI_CSI) {
            if (character >= 0x40 && character <= 0x7e) filter->state = WPM_ANSI_TEXT;
        }
        else if (filter->state == WPM_ANSI_OSC) {
            if (character == 0x07) filter->state = WPM_ANSI_TEXT;
            else if (character == 0x1b) filter->state = WPM_ANSI_OSC_ESCAPE;
        }
        else if (filter->state == WPM_ANSI_OSC_ESCAPE) {
            filter->state = character == '\\' ? WPM_ANSI_TEXT : WPM_ANSI_OSC;
        }
    }
    return written;
}

static void write_script_bytes(const unsigned char* bytes, DWORD count, FILE* log,
                               FILE* handoff_log, wpm_script_terminal* terminal,
                               wpm_script_log_filter* filter) {
    unsigned char filtered[4096];
    DWORD filtered_count;
    if (count == 0) return;
    write_script_terminal(bytes, count, terminal);
    filtered_count = filter_script_log(bytes, count, filtered, filter);
    fwrite(filtered, 1, filtered_count, log);
    fflush(log);
    if (handoff_log) {
        fwrite(filtered, 1, filtered_count, handoff_log);
        fflush(handoff_log);
    }
}

static void report_script_failure(const char* action_name, DWORD exit_code,
                                  const char* repository_url, const char* log_path) {
    char github_url[WPM_PATH_SIZE];
    char* suffix;
    printf("Error: %s script failed with exit code %lu.\n", action_name,
        (unsigned long)exit_code);
    printf("Script log: %s\n", log_path);
    if (!repository_url) {
        printf("Repository URL: unavailable for this local or legacy package.\n");
        printf("Please create an issue with the package maintainer and attach the script log.\n");
        return;
    }
    printf("Repository URL: %s\n", repository_url);
    if (_strnicmp(repository_url, "https://github.com/", 19) == 0 &&
        strlen(repository_url) < sizeof(github_url)) {
        strcpy_s(github_url, sizeof(github_url), repository_url);
        suffix = strstr(github_url + 19, "/releases/");
        if (suffix) *suffix = '\0';
        suffix = strstr(github_url + 19, "/issues");
        if (suffix) *suffix = '\0';
        if (strlen(github_url) + strlen("/issues/new") < sizeof(github_url)) {
            strcat(github_url, "/issues/new");
            printf("Please create a GitHub issue for this failure and attach the script log:\n  %s\n",
                github_url);
            return;
        }
    }
    printf("Please create an issue in the repository above and attach the script log.\n");
}

static int run_package_script(
    const char* staging_dir,
    const char* script_name,
    const char* action_name,
    const char* package_name,
    DWORD* result_exit_code
) {
    char script_path[WPM_PATH_SIZE];
    char command_line[WPM_PATH_SIZE * 2];
    char self_upgrade_log[WPM_PATH_SIZE];
    char inherited_repository[WPM_PATH_SIZE];
    char log_path[WPM_PATH_SIZE];
    const char* repository_url;
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;
    SECURITY_ATTRIBUTES pipe_security;
    HANDLE pipe_read = NULL;
    HANDLE pipe_write = NULL;
    FILE* log = NULL;
    FILE* handoff_log = NULL;
    wpm_script_terminal terminal;
    wpm_script_log_filter log_filter;
    DWORD exit_code;
    DWORD wait_result;
    DWORD elapsed = 0;
    DWORD next_report = 10000;
    DWORD available;
    DWORD bytes_read;
    unsigned char output[4096];
    int added_force_color = 0;
    int added_clicolor_force = 0;
    int written;

    if (!join_path(script_path, sizeof(script_path), staging_dir, script_name)) {
        printf("Error: %s script path is too long.\n", action_name);
        return 0;
    }
    if (result_exit_code) *result_exit_code = 0;
    if (!file_exists_at_path(script_path)) return 1;

    verbose_log("Running %s script: %s", action_name, script_path);
    repository_url = active_repository_url(inherited_repository, sizeof(inherited_repository));
    if (!create_script_log(package_name, action_name, log_path, sizeof(log_path), &log)) {
        printf("Error: could not create the %s script log.\n", action_name);
        return 0;
    }
    fprintf(log, "action=%s\npackage=%s\nscript=%s\nrepository=%s\n--- output ---\n",
        action_name, package_name, script_path, repository_url ? repository_url : "unavailable");
    fflush(log);
    printf("Logging %s script output to: %s\n", action_name, log_path);
    if (GetEnvironmentVariableA("WPM_SELF_UPGRADE_LOG", self_upgrade_log,
            sizeof(self_upgrade_log)) > 0 && _stricmp(self_upgrade_log, log_path) != 0) {
        handoff_log = wpm_fopen(self_upgrade_log, "ab");
    }

    written = snprintf(command_line, sizeof(command_line),
        "cmd.exe /d /s /c call \"%s\"", script_path);
    if (written < 0 || (size_t)written >= sizeof(command_line)) {
        printf("Error: %s command is too long.\n", action_name);
        if (handoff_log) fclose(handoff_log);
        fclose(log);
        return 0;
    }

    memset(&pipe_security, 0, sizeof(pipe_security));
    pipe_security.nLength = sizeof(pipe_security);
    pipe_security.bInheritHandle = TRUE;
    if (!CreatePipe(&pipe_read, &pipe_write, &pipe_security, 0) ||
        !SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0)) {
        printf("Error: could not create the %s script output stream.\n", action_name);
        if (pipe_read) CloseHandle(pipe_read);
        if (pipe_write) CloseHandle(pipe_write);
        if (handoff_log) fclose(handoff_log);
        fclose(log);
        return 0;
    }

    memset(&startup_info, 0, sizeof(startup_info));
    memset(&process_info, 0, sizeof(process_info));
    memset(&log_filter, 0, sizeof(log_filter));
    initialize_script_terminal(&terminal);
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = pipe_write;
    startup_info.hStdError = pipe_write;
    printf("--- package %s: %s script output ---\n", package_name, action_name);
    fflush(stdout);
    if (terminal.interactive && getenv("NO_COLOR") == NULL) {
        if (getenv("FORCE_COLOR") == NULL &&
            SetEnvironmentVariableA("FORCE_COLOR", "1")) added_force_color = 1;
        if (getenv("CLICOLOR_FORCE") == NULL &&
            SetEnvironmentVariableA("CLICOLOR_FORCE", "1")) added_clicolor_force = 1;
    }
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
        if (added_force_color) SetEnvironmentVariableA("FORCE_COLOR", NULL);
        if (added_clicolor_force) SetEnvironmentVariableA("CLICOLOR_FORCE", NULL);
        printf("Error: could not start %s script: %s\n", action_name, script_path);
        CloseHandle(pipe_read);
        CloseHandle(pipe_write);
        if (handoff_log) fclose(handoff_log);
        fclose(log);
        return 0;
    }
    if (added_force_color) SetEnvironmentVariableA("FORCE_COLOR", NULL);
    if (added_clicolor_force) SetEnvironmentVariableA("CLICOLOR_FORCE", NULL);
    CloseHandle(pipe_write);
    pipe_write = NULL;

    verbose_log("WPM process PID: %lu", (unsigned long)GetCurrentProcessId());
    verbose_log("%s script process PID: %lu", action_name,
        (unsigned long)process_info.dwProcessId);
    verbose_log("WPM PID %lu is waiting for %s script PID %lu",
        (unsigned long)GetCurrentProcessId(), action_name,
        (unsigned long)process_info.dwProcessId);
    do {
        if (PeekNamedPipe(pipe_read, NULL, 0, NULL, &available, NULL) && available > 0 &&
            ReadFile(pipe_read, output, available < sizeof(output) ? available : sizeof(output),
                &bytes_read, NULL)) {
            write_script_bytes(output, bytes_read, log, handoff_log, &terminal,
                &log_filter);
        }
        wait_result = WaitForSingleObject(process_info.hProcess, 100);
        if (wait_result == WAIT_TIMEOUT) {
            elapsed += 100;
        }
        if (wpm_verbose && wait_result == WAIT_TIMEOUT && elapsed >= next_report) {
            verbose_log("%s script PID %lu is still running after %lu seconds",
                action_name, (unsigned long)process_info.dwProcessId,
                (unsigned long)(elapsed / 1000));
            report_script_children(process_info.dwProcessId);
            verbose_log("Debug with: tasklist /FI \"PID eq %lu\" /V",
                (unsigned long)process_info.dwProcessId);
            next_report += 30000;
        }
    } while (wait_result == WAIT_TIMEOUT);
    while (PeekNamedPipe(pipe_read, NULL, 0, NULL, &available, NULL) && available > 0 &&
        ReadFile(pipe_read, output, available < sizeof(output) ? available : sizeof(output),
            &bytes_read, NULL)) {
        write_script_bytes(output, bytes_read, log, handoff_log, &terminal,
            &log_filter);
    }
    if (!GetExitCodeProcess(process_info.hProcess, &exit_code)) exit_code = 1;
    if (terminal.interactive) {
        SetConsoleTextAttribute(terminal.output, terminal.default_attributes);
    }
    printf("--- end package %s: %s script output (exit code %lu) ---\n",
        package_name, action_name, (unsigned long)exit_code);
    fprintf(log, "\n--- exit-code=%lu ---\n", (unsigned long)exit_code);
    if (result_exit_code) *result_exit_code = exit_code;
    CloseHandle(pipe_read);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    if (handoff_log) fclose(handoff_log);
    fclose(log);
    if (exit_code != 0) {
        report_script_failure(action_name, exit_code, repository_url, log_path);
        return 0;
    }

    return 1;
}

int wpm_archive_inspect(const char* archive_path, wpm_package_info* info) {
    wpm_package_metadata metadata;
    if (!archive_path || !info || !read_archive_package_metadata(archive_path, &metadata)) return 0;
    strcpy_s(info->name, sizeof(info->name), metadata.name);
    strcpy_s(info->version, sizeof(info->version), metadata.version);
    strcpy_s(info->arch, sizeof(info->arch), metadata.arch);
    strcpy_s(info->archive_name, sizeof(info->archive_name), path_basename(archive_path));
    return 1;
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
        !create_directories(temp_root) || !remove_directory_tree_with_retry(staging_path)) {
        printf("Error: could not prepare package verification staging.\n");
        return 0;
    }

    if (!wpm_archive_extract_with_label(
            archive_full_path, staging_path, path_basename(archive_full_path))) goto cleanup;
    if (!wpm_validate_package_signature(staging_path, 0, signing_key_id, sizeof(signing_key_id))) goto cleanup;
    if (!verify_package_index(staging_path, path_basename(archive_full_path))) goto cleanup;
    if (!read_package_metadata(staging_path, &metadata)) goto cleanup;
    success = 1;

cleanup:
    if (!remove_directory_tree_with_retry(staging_path)) {
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
    wpm_package_metadata display_metadata;
    const char* display_name;
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
    display_name = read_archive_package_metadata(archive_full_path, &display_metadata)
        ? display_metadata.name : package_name;

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
    if (!remove_directory_tree_with_retry(staging_path)) {
        printf("Error: could not clear staging directory: %s\n", staging_path);
        return 0;
    }

    verbose_log("Using staging directory: %s", staging_path);

    print_package_progress(display_name, "Extracting package");
    if (!wpm_archive_extract_with_label(archive_full_path, staging_path, display_name)) goto cleanup;
    print_package_progress(display_name, "Validating package");
    if (!wpm_validate_package_signature(staging_path, allow_unsigned, signing_key_id, sizeof(signing_key_id))) goto cleanup;
    if (!verify_package_index(staging_path, display_name)) goto cleanup;
    if (!read_package_metadata(staging_path, &metadata)) goto cleanup;
    verbose_log("Resolved package identity: %s %s %s", metadata.name,
        metadata.arch, metadata.version);
    if (!installed_architecture_is_compatible(&metadata)) goto cleanup;
    print_package_progress(display_name, "Installing package");
    if (!run_package_script(staging_path, ".wpm\\install.cmd", "install", metadata.name, NULL)) goto cleanup;
    if (_stricmp(archive_full_path, stored_archive_path) != 0) {
        verbose_log("Storing archive: %s", stored_archive_path);
    }
    if (_stricmp(archive_full_path, stored_archive_path) != 0 &&
        !CopyFileA(archive_full_path, stored_archive_path, FALSE)) {
        printf("Error: could not store package archive: %s\n", stored_archive_path);
        goto cleanup;
    }
    if (!record_archive_repository(stored_archive_path)) {
        printf("Warning: could not retain the package repository URL for removal diagnostics.\n");
    }
    if (!write_installation_audit(data_root, path_basename(archive_full_path), &metadata, signing_key_id)) {
        printf("Error: could not record package verification audit.\n");
        goto cleanup;
    }

    success = 1;

cleanup:
    if (!remove_directory_tree_with_retry(staging_path)) {
        printf("Error: could not remove staging directory: %s\n", staging_path);
        success = 0;
    }
    if (!success) return 0;

    printf("Installed package; archive stored at: %s\n", stored_archive_path);
    printf("Result: %s %s installed\n", metadata.name, metadata.arch);
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
    char stage[WPM_PATH_SIZE], staged_exe[WPM_PATH_SIZE], staged_runtime[WPM_PATH_SIZE], cache[WPM_PATH_SIZE];
    char handoff_root[WPM_PATH_SIZE], handoff_dir[WPM_PATH_SIZE];
    char handoff_exe[WPM_PATH_SIZE], handoff_runtime[WPM_PATH_SIZE];
    char audit_dir[WPM_PATH_SIZE], log_path[WPM_PATH_SIZE];
    char previous_repository[WPM_PATH_SIZE];
    char signing_key[65] = "", command[WPM_PATH_SIZE * 2];
    wpm_package_metadata metadata;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD previous_repository_length;
    BOOL process_created;
    int result = 0;
    if (!normalized_full_path(archive_path, archive_full, sizeof(archive_full)) ||
        !wpm_get_data_root(root, sizeof(root)) || !join_path(temp, sizeof(temp), root, "temp") ||
        snprintf(stage, sizeof(stage), "%s\\self-upgrade-stage-%lu", temp,
            (unsigned long)GetCurrentProcessId()) < 0 ||
        !join_path(staged_exe, sizeof(staged_exe), stage, "wpm.exe") ||
        !join_path(staged_runtime, sizeof(staged_runtime), stage, "wcrt.dll") ||
        !join_path(cache, sizeof(cache), root, "cache") ||
        !join_path(handoff_root, sizeof(handoff_root), cache, "self-upgrade") ||
        snprintf(handoff_dir, sizeof(handoff_dir), "%s\\%s-%s-%lu", handoff_root,
            expected_arch, expected_version, (unsigned long)GetCurrentProcessId()) < 0 ||
        !join_path(handoff_exe, sizeof(handoff_exe), handoff_dir, "wpm.exe") ||
        !join_path(handoff_runtime, sizeof(handoff_runtime), handoff_dir, "wcrt.dll") ||
        !join_path(audit_dir, sizeof(audit_dir), root, "audit") ||
        snprintf(log_path, sizeof(log_path), "%s\\self-upgrade-%s-%s.log", audit_dir,
            expected_arch, expected_version) < 0 ||
        !create_directories(temp) || !create_directories(cache) || !create_directories(audit_dir) ||
        !create_directories(handoff_root) || !create_directories(handoff_dir) ||
        !remove_directory_tree_with_retry(stage)) return 0;
    DeleteFileA(log_path);
    memset(&metadata, 0, sizeof(metadata));
    strcpy_s(metadata.name, sizeof(metadata.name), "wpm");
    strcpy_s(metadata.version, sizeof(metadata.version), expected_version);
    strcpy_s(metadata.arch, sizeof(metadata.arch), expected_arch);
    printf("Self-upgrade stage 1 of 2: the installed WPM verifies the new %s package "
        "before launching it to finish the upgrade.\n", expected_version);
    print_package_progress("wpm", "Extracting package");
    if (!wpm_archive_extract_with_label(archive_full, stage, "wpm")) {
        write_upgrade_audit(root, &metadata, old_version, path_basename(archive_full), signing_key, 1, "self-upgrade-extraction", 0);
        goto cleanup;
    }
    print_package_progress("wpm", "Validating package");
    if (!wpm_validate_package_signature(stage, allow_unsigned, signing_key, sizeof(signing_key)) ||
        !verify_package_index(stage, "wpm") || !read_package_metadata(stage, &metadata)) {
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
        printf("Error: could not cache the WPM self-upgrade executable (Windows error %lu).\n",
            (unsigned long)GetLastError());
        goto cleanup;
    }
    if (file_exists_at_path(staged_runtime) && !CopyFileA(staged_runtime, handoff_runtime, FALSE)) {
        printf("Error: could not cache the optional WPM runtime dependency (Windows error %lu).\n",
            (unsigned long)GetLastError());
        goto cleanup;
    }
    if (snprintf(command, sizeof(command), "\"%s\" --complete-self-upgrade \"%s\" %lu \"%s\" \"%s\" \"%s\" %d \"%s\"%s",
        handoff_exe, archive_full, (unsigned long)GetCurrentProcessId(), expected_version,
        expected_arch, old_version, allow_unsigned ? 1 : 0, log_path,
        wpm_verbose ? " --verbose" : "") < 0) goto cleanup;
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    previous_repository_length = GetEnvironmentVariableA("WPM_PACKAGE_REPOSITORY_URL",
        previous_repository, sizeof(previous_repository));
    if (wpm_repository_url[0]) {
        SetEnvironmentVariableA("WPM_PACKAGE_REPOSITORY_URL", wpm_repository_url);
    }
    process_created = CreateProcessA(handoff_exe, command, NULL, NULL, TRUE,
        CREATE_NEW_PROCESS_GROUP, NULL, NULL, &startup, &process);
    if (previous_repository_length > 0 && previous_repository_length < sizeof(previous_repository)) {
        SetEnvironmentVariableA("WPM_PACKAGE_REPOSITORY_URL", previous_repository);
    }
    else {
        SetEnvironmentVariableA("WPM_PACKAGE_REPOSITORY_URL", NULL);
    }
    if (!process_created) {
        printf("Error: could not launch the cached WPM self-upgrade executable.\n");
        goto cleanup;
    }
    verbose_log("Self-upgrade invoking process PID: %lu", (unsigned long)GetCurrentProcessId());
    verbose_log("Self-upgrade completion process PID: %lu", (unsigned long)process.dwProcessId);
    verbose_log("Completion process will wait for PID %lu to exit",
        (unsigned long)GetCurrentProcessId());
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    printf("Scheduled WPM self-upgrade to %s; installation will continue after this process exits.\n", expected_version);
    printf("Self-upgrade output: %s\n", log_path);
    result = 1;
cleanup:
    if (!remove_directory_tree_with_retry(stage)) result = 0;
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
        !create_directories(store) || !remove_directory_tree_with_retry(stage)) return 0;
    print_package_progress(expected_name, "Extracting package");
    if (!wpm_archive_extract_with_label(archive_full, stage, expected_name)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "extraction", 0);
        goto cleanup;
    }
    print_package_progress(expected_name, "Validating package");
    if (!wpm_validate_package_signature(stage, allow_unsigned, signing_key, sizeof(signing_key))) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "signature-validation", 0);
        goto cleanup;
    }
    if (!verify_package_index(stage, expected_name)) {
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
    print_package_progress(expected_name, "Installing package");
    if (!run_package_script(stage, ".wpm\\install.cmd", "upgrade install", metadata.name, &script_exit)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "install-script", script_exit);
        printf("Warning: package-maintainer recovery may be required.\n");
        goto cleanup;
    }
    if (_stricmp(archive_full, stored) != 0 && !CopyFileA(archive_full, stored, FALSE)) {
        write_upgrade_audit(root, &metadata, old_version, base, signing_key, 1, "archive-retention", 0);
        printf("Error: upgrade deployed software but archive retention failed; recovery is required.\n");
        goto cleanup;
    }
    if (!record_archive_repository(stored)) {
        printf("Warning: could not retain the package repository URL for removal diagnostics.\n");
    }
    if (!write_upgrade_audit(root, &metadata, old_version, base, signing_key, 0, NULL, 0)) {
        printf("Error: upgrade deployed software but audit recording failed; recovery is required.\n");
        goto cleanup;
    }
    success = 1;
cleanup:
    if (!remove_directory_tree_with_retry(stage)) success = 0;
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
    char repository_url[WPM_PATH_SIZE];
    wpm_package_metadata metadata;
    char* extension;
    int written;
    int script_success;
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
    verbose_log("Removing archive: %s", stored_archive_path);
    if (!create_directories(temp_root) || !remove_directory_tree_with_retry(staging_path)) {
        printf("Error: could not prepare removal staging directory.\n");
        return 0;
    }
    verbose_log("Using removal staging directory: %s", staging_path);

    if (!wpm_archive_extract_with_label(stored_archive_path, staging_path, archive_name)) goto cleanup;
    if (!verify_package_index(staging_path, archive_name)) goto cleanup;
    if (!read_package_metadata(staging_path, &metadata)) goto cleanup;
    verbose_log("Resolved package identity: %s %s %s", metadata.name,
        metadata.arch, metadata.version);
    wpm_archive_set_repository_url(NULL);
    if (load_archive_repository(stored_archive_path, repository_url, sizeof(repository_url))) {
        wpm_archive_set_repository_url(repository_url);
    }
    script_success = run_package_script(staging_path, ".wpm\\remove.cmd", "removal",
        archive_name, NULL);
    wpm_archive_set_repository_url(NULL);
    if (!script_success) goto cleanup;
    verbose_log("Deleting retained archive: %s", stored_archive_path);
    if (!DeleteFileA(stored_archive_path)) {
        printf("Error: could not remove stored package archive: %s\n", stored_archive_path);
        goto cleanup;
    }
    if (repository_record_path(stored_archive_path, repository_url, sizeof(repository_url))) {
        DeleteFileA(repository_url);
    }
    success = 1;

cleanup:
    if (!remove_directory_tree_with_retry(staging_path)) {
        printf("Error: could not remove staging directory: %s\n", staging_path);
        success = 0;
    }
    if (!success) return 0;

    printf("Removed package: %s\n", archive_name);
    printf("Result: %s %s removed\n", metadata.name, metadata.arch);
    return 1;
}
