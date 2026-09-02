/** @file repository.c @brief Repository and package resolution operations. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <windows.h>
#ifdef __TINYC__
# include "tcc_support/urlmon.h"
#else
# include <urlmon.h>
#endif
#include "archive.h"
#include "helpers.h"
#include "repository.h"
#include "logging.h"
#include "progress.h"

#define PATH_SIZE 4096
#define MAX_REPOSITORIES 32
#define MAX_PACKAGES 96
#define MAX_INSTALLED 256
#define INDEX_FRESH_MS (60ULL * 60ULL * 1000ULL)
/* URLMon status omitted by TinyCC's compact Windows headers. */
#define WPM_BINDSTATUS_64BIT_PROGRESS 56UL
#define WPM_BINDSTATUS_REDIRECTING 3UL
#define WPM_DEFAULT_REPOSITORY "https://github.com/Thewafflication/wpm/releases/latest/download"

typedef struct {
    char url[PATH_SIZE];
    int priority;
    int order;
    int allow_insecure_http;
} repository;
typedef struct { char name[128]; char version[64]; char arch[16]; char url[PATH_SIZE]; int priority; int order; } package_entry;

typedef struct download_callback download_callback;
typedef struct {
    HRESULT (STDMETHODCALLTYPE *query_interface)(download_callback*, const GUID*, void**);
    ULONG (STDMETHODCALLTYPE *add_ref)(download_callback*);
    ULONG (STDMETHODCALLTYPE *release)(download_callback*);
    HRESULT (STDMETHODCALLTYPE *on_start_binding)(download_callback*, DWORD, void*);
    HRESULT (STDMETHODCALLTYPE *get_priority)(download_callback*, LONG*);
    HRESULT (STDMETHODCALLTYPE *on_low_resource)(download_callback*, DWORD);
    HRESULT (STDMETHODCALLTYPE *on_progress)(download_callback*, ULONG, ULONG, ULONG, LPCWSTR);
    HRESULT (STDMETHODCALLTYPE *on_stop_binding)(download_callback*, HRESULT, LPCWSTR);
    HRESULT (STDMETHODCALLTYPE *get_bind_info)(download_callback*, DWORD*, void*);
    HRESULT (STDMETHODCALLTYPE *on_data_available)(download_callback*, DWORD, DWORD, void*, void*);
    HRESULT (STDMETHODCALLTYPE *on_object_available)(download_callback*, const GUID*, void*);
} download_callback_vtable;

struct download_callback {
    const download_callback_vtable* vtable;
    ULONG references;
    wpm_progress progress;
    int using_64bit_progress;
    int insecure_http;
    char initial_url[PATH_SIZE];
};

static int repository_verbose = 0;

void wpm_repo_set_verbose(int enabled) { repository_verbose = enabled != 0; }

static void repository_verbose_log(const char* format, ...)
{
    va_list arguments;
    char message[8192];
    if (!repository_verbose) return;
    va_start(arguments, format);
    if (vsnprintf(message, sizeof(message), format, arguments) >= 0) {
        message[sizeof(message) - 1] = '\0';
        printf("Verbose: Repository: %s\n", message);
    }
    va_end(arguments);
}

#define REPO_VERBOSE(...) repository_verbose_log(__VA_ARGS__)

static int join_path(char* result, size_t size, const char* left, const char* right) {
    int n = snprintf(result, size, "%s%s%s", left, left[strlen(left) - 1] == '\\' ? "" : "\\", right);
    return n >= 0 && (size_t)n < size;
}
static int ensure_directory(const char* path) {
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) ||
        CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}
static int ensure_cache_directory(const char* leaf) {
    char root[PATH_SIZE], cache[PATH_SIZE], path[PATH_SIZE];
    return wpm_get_data_root(root, sizeof(root)) && join_path(cache, sizeof(cache), root, "cache") &&
        join_path(path, sizeof(path), cache, leaf) && ensure_directory(path);
}
static int config_path(char* path, size_t size) { char root[PATH_SIZE]; return wpm_get_data_root(root, sizeof(root)) && join_path(path, size, root, "config\\repositories.txt"); }
static int cache_path(const char* url, char* path, size_t size) {
    unsigned long hash = 2166136261u; char root[PATH_SIZE]; const unsigned char* p = (const unsigned char*)url;
    while (*p) { hash ^= *p++; hash *= 16777619u; }
    return wpm_get_data_root(root, sizeof(root)) && snprintf(path, size, "%s\\cache\\repositories\\%08lx.json", root, hash) > 0;
}
static int is_https_repository(const char* source) {
    return source && _strnicmp(source, "https://", 8) == 0;
}
static int is_http_repository(const char* source) {
    return source && _strnicmp(source, "http://", 7) == 0;
}
static int is_web_repository(const char* source) {
    return is_https_repository(source) || is_http_repository(source);
}
static size_t web_origin_length(const char* source) {
    const char* authority;
    const char* end;
    size_t scheme_length;
    if (is_https_repository(source)) scheme_length = 8;
    else if (is_http_repository(source)) scheme_length = 7;
    else return 0;
    authority = source + scheme_length;
    end = strchr(authority, '/');
    return end ? (size_t)(end - source) : strlen(source);
}
static int same_web_origin(const char* left, const char* right) {
    size_t left_length = web_origin_length(left);
    size_t right_length = web_origin_length(right);
    return left_length && left_length == right_length &&
        _strnicmp(left, right, left_length) == 0;
}
static int web_locator_valid(const char* source, size_t scheme_length) {
    const char* authority = source + scheme_length;
    const char* end = authority;
    if (!*authority || strchr(authority, '?') || strchr(authority, '#')) return 0;
    while (*end && *end != '/') {
        if (*end == '\\' || *end == '@' || *end == '?' || *end == '#') return 0;
        end++;
    }
    return end > authority;
}
static int unc_locator_valid(const char* source) {
    const char* server;
    const char* share;
    const char* end;
    if (!source || source[0] != '\\' || source[1] != '\\' ||
        source[2] == '.' || source[2] == '?' || source[2] == '\\') return 0;
    server = source + 2;
    share = strchr(server, '\\');
    if (!share || share == server || !share[1]) return 0;
    end = strchr(share + 1, '\\');
    if (end == share + 1) return 0;
    return 1;
}
static int repository_normalize(const char* source, char* result, size_t size,
    int report_error) {
    size_t length;
    DWORD required;
    if (!source || !*source || strpbrk(source, "\t\r\n")) {
        if (report_error) {
            printf("Error: repository locator is empty or contains a "
                "control character.\n");
        }
        return 0;
    }
    if (is_web_repository(source)) {
        if (strpbrk(source, " \t\r\n") || strcpy_s(result, size, source) != 0) return 0;
        if (!web_locator_valid(result, is_https_repository(result) ? 8 : 7)) {
            if (report_error) {
                printf("Error: repository URL requires an authority and "
                    "cannot contain credentials.\n");
            }
            return 0;
        }
        length = strlen(result);
        while (length > (is_https_repository(result) ? 8u : 7u) &&
            result[length - 1] == '/') result[--length] = '\0';
        return 1;
    }
    if (strstr(source, "://")) {
        if (report_error) {
            printf("Error: repository locators must use https://, opted-in "
                "http://, or a filesystem path.\n");
        }
        return 0;
    }
    required = GetFullPathNameA(source, (DWORD)size, result, NULL);
    if (!required || required >= size) {
        if (report_error) {
            printf("Error: repository path is invalid or too long: %s\n",
                source);
        }
        return 0;
    }
    for (length = 0; result[length]; length++) if (result[length] == '/') result[length] = '\\';
    if (result[0] == '\\' && result[1] == '\\') {
        if (!unc_locator_valid(result)) {
            if (report_error) {
                printf("Error: UNC repositories require a server and share "
                    "and cannot use a device namespace.\n");
            }
            return 0;
        }
        while (length > 2 && result[length - 1] == '\\') result[--length] = '\0';
        return 1;
    }
    if (length < 3 || !isalpha((unsigned char)result[0]) ||
        result[1] != ':' || result[2] != '\\') {
        if (report_error) {
            printf("Error: filesystem repositories require a drive-qualified "
                "or UNC path.\n");
        }
        return 0;
    }
    if (strchr(result + 2, ':')) {
        if (report_error) {
            printf("Error: filesystem repository paths cannot contain a "
                "colon after the drive prefix.\n");
        }
        return 0;
    }
    while (length > 3 && result[length - 1] == '\\') result[--length] = '\0';
    return 1;
}
static int parse_entry(char* line, int* priority, char** url, int* allow_http) {
    char* tab = strchr(line, '\t'); char* option; char* end;
    if (!tab) return 0; *tab = '\0'; *priority = (int)strtol(line, &end, 10); if (*end) return 0;
    *url = tab + 1; (*url)[strcspn(*url, "\r\n")] = '\0';
    option = strchr(*url, '\t');
    *allow_http = 0;
    if (option) {
        *option++ = '\0';
        if (strcmp(option, "allow-insecure-http=true") != 0) return 0;
        *allow_http = 1;
    }
    return **url != '\0' && (!is_http_repository(*url) || *allow_http);
}
static int load_repositories(repository* result, int* count) {
    char path[PATH_SIZE], line[PATH_SIZE + 32], normalized[PATH_SIZE];
    FILE* input;
    *count = 0;
    if (!config_path(path, sizeof(path))) return 0; REPO_VERBOSE("configuration: %s", path); input = wpm_fopen(path, "r");
    if (input) {
        while (*count < MAX_REPOSITORIES && fgets(line, sizeof(line), input)) { char* url; int priority, allow_http;
            if (parse_entry(line, &priority, &url, &allow_http)) {
                if (!repository_normalize(url, normalized, sizeof(normalized), 0) ||
                    (!is_web_repository(url) && _stricmp(url, normalized) != 0)) {
                    if (!is_web_repository(url)) {
                        printf("Warning: ignoring invalid configured filesystem "
                            "repository locator: %s\n"
                            "  Remove it with: wpm repo remove \"%s\"\n",
                            url, url);
                    } else {
                        printf("Warning: ignoring an invalid configured web "
                            "repository locator. Remove or re-add it with "
                            "'wpm repo'.\n");
                    }
                    continue;
                }
                { repository* r = &result[*count]; r->priority = priority; r->order = *count; r->allow_insecure_http = allow_http; strcpy_s(r->url, sizeof(r->url), normalized); REPO_VERBOSE("configured[%d]: priority=%d transport=%s url=%s", *count, priority, allow_http ? "insecure-http" : "standard", normalized); (*count)++; }
            }
        }
        fclose(input);
    }
    for (int i = 0; i < *count; i++) if (_stricmp(result[i].url, WPM_DEFAULT_REPOSITORY) == 0) { REPO_VERBOSE("loaded %d repositories", *count); return 1; }
    if (*count < MAX_REPOSITORIES) { repository* r = &result[*count]; r->priority = 0; r->order = *count; r->allow_insecure_http = 0; strcpy_s(r->url, sizeof(r->url), WPM_DEFAULT_REPOSITORY); REPO_VERBOSE("built-in[%d]: priority=0 url=%s", *count, WPM_DEFAULT_REPOSITORY); (*count)++; }
    REPO_VERBOSE("loaded %d repositories", *count);
    return 1;
}
static int write_entry(FILE* output, int priority, const char* url, int allow_http) {
    return fprintf(output, "%d\t%s%s\n", priority, url,
        allow_http ? "\tallow-insecure-http=true" : "") >= 0;
}
static int rewrite(const char* wanted, int priority, int allow_http, int remove) {
    char path[PATH_SIZE], temporary[PATH_SIZE], line[PATH_SIZE + 32]; FILE *input, *output; int found = 0;
    if (!config_path(path, sizeof(path)) || snprintf(temporary, sizeof(temporary), "%s.tmp", path) < 0) return 0;
    input = wpm_fopen(path, "r"); output = wpm_fopen(temporary, "w");
    if (!output) { if (input) fclose(input); printf("Error: could not write repository configuration.\n"); return 0; }
    if (input) { while (fgets(line, sizeof(line), input)) { char original[PATH_SIZE + 32], *url; int old_priority, old_allow_http; strcpy_s(original, sizeof(original), line);
        if (parse_entry(line, &old_priority, &url, &old_allow_http) && _stricmp(url, wanted) == 0) { found = 1; if (!remove) write_entry(output, priority, wanted, allow_http); } else fputs(original, output); } fclose(input); }
    if (!remove && !found) write_entry(output, priority, wanted, allow_http);
    if (fclose(output) || !MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING)) { DeleteFileA(temporary); printf("Error: could not save repository configuration.\n"); return 0; }
    if (remove && !found) { printf("Error: repository is not configured: %s\n", wanted); return 0; }
    printf("Repository %s: %s\n", remove ? "removed" : (found ? "updated" : "added"), wanted); return 1;
}
int wpm_repo_add(const char* url, int priority, int allow_insecure_http) {
    char normalized[PATH_SIZE];
    if (!repository_normalize(url, normalized, sizeof(normalized), 1)) return 0;
    if (is_http_repository(normalized) && !allow_insecure_http) {
        printf("Error: plain HTTP repositories require --allow-insecure-http.\n");
        return 0;
    }
    if (!is_http_repository(normalized) && allow_insecure_http) {
        printf("Error: --allow-insecure-http is valid only for an http:// repository.\n");
        return 0;
    }
    if (allow_insecure_http) {
        printf("Warning: insecure HTTP transport exposes repository metadata and downloads; "
            "package signatures and trust checks remain required.\n");
    }
    return rewrite(normalized, priority, allow_insecure_http, 0);
}
int wpm_repo_remove(const char* url) {
    char normalized[PATH_SIZE], cached[PATH_SIZE];
    const char* wanted;
    if (repository_normalize(url, normalized, sizeof(normalized), 0)) {
        wanted = normalized;
    } else {
        if (!url || !*url || strstr(url, "://") || strpbrk(url, "\t\r\n")) {
            repository_normalize(url, normalized, sizeof(normalized), 1);
            return 0;
        }
        wanted = url;
    }
    if (!rewrite(wanted, 0, 0, 1)) return 0;
    if (cache_path(wanted, cached, sizeof(cached))) DeleteFileA(cached);
    return 1;
}
int wpm_repo_list(void) { repository repositories[MAX_REPOSITORIES]; int count, i; if (!load_repositories(repositories, &count)) return 0; if (!count) { printf("No repositories configured.\n"); return 1; } for (i = 0; i < count; i++) printf("%d\t%s%s\n", repositories[i].priority, repositories[i].url, repositories[i].allow_insecure_http ? "\t[insecure HTTP allowed]" : ""); return 1; }

static int download_guid_equal(const GUID* left, const GUID* right) {
    return left != NULL && right != NULL && memcmp(left, right, sizeof(*left)) == 0;
}

static HRESULT STDMETHODCALLTYPE download_query_interface(
    download_callback* callback, const GUID* interface_id, void** result) {
    static const GUID unknown_id =
        { 0x00000000UL, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
    static const GUID bind_status_callback_id =
        { 0x79eac9c1UL, 0xbaf9, 0x11ce, { 0x8c, 0x82, 0x00, 0xaa, 0x00, 0x4b, 0xa9, 0x0b } };
    if (!result) return E_POINTER;
    *result = NULL;
    if (!download_guid_equal(interface_id, &unknown_id) &&
        !download_guid_equal(interface_id, &bind_status_callback_id)) return E_NOINTERFACE;
    *result = callback;
    callback->references++;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE download_add_ref(download_callback* callback) {
    return ++callback->references;
}

static ULONG STDMETHODCALLTYPE download_release(download_callback* callback) {
    if (callback->references) callback->references--;
    return callback->references;
}

static HRESULT STDMETHODCALLTYPE download_on_start_binding(
    download_callback* callback, DWORD reserved, void* binding) {
    (void)callback; (void)reserved; (void)binding; return S_OK;
}

static HRESULT STDMETHODCALLTYPE download_get_priority(download_callback* callback, LONG* priority) {
    (void)callback; (void)priority; return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE download_on_low_resource(download_callback* callback, DWORD reserved) {
    (void)callback; (void)reserved; return S_OK;
}

static int download_parse_64bit_progress(const wchar_t* text,
    unsigned long long* current, unsigned long long* total) {
    const wchar_t* cursor = text;
    unsigned long long values[2] = { 0, 0 };
    int value_index;
    if (!text || !current || !total) return 0;
    for (value_index = 0; value_index < 2; value_index++) {
        const wchar_t* start = cursor;
        while (*cursor >= L'0' && *cursor <= L'9') {
            unsigned digit = (unsigned)(*cursor - L'0');
            if (values[value_index] > (ULLONG_MAX - digit) / 10ULL) return 0;
            values[value_index] = values[value_index] * 10ULL + digit;
            cursor++;
        }
        if (cursor == start) return 0;
        if (value_index == 0) {
            if (*cursor++ != L',') return 0;
        } else if (*cursor != L'\0') {
            return 0;
        }
    }
    *current = values[0];
    *total = values[1];
    return 1;
}

static HRESULT STDMETHODCALLTYPE download_on_progress(download_callback* callback,
    ULONG current, ULONG total, ULONG status, LPCWSTR status_text) {
    unsigned long long current_64;
    unsigned long long total_64;
    if (status == WPM_BINDSTATUS_REDIRECTING && status_text) {
        char redirect[PATH_SIZE];
        int converted = WideCharToMultiByte(CP_UTF8, 0, status_text, -1,
            redirect, sizeof(redirect), NULL, NULL);
        if (!converted || !is_web_repository(redirect) ||
            (callback->insecure_http &&
                (!is_http_repository(redirect) ||
                    !same_web_origin(callback->initial_url, redirect))) ||
            (!callback->insecure_http && is_http_repository(redirect))) {
            printf("Error: repository redirect changed to a disallowed origin or scheme.\n");
            return E_ABORT;
        }
    }
    if (status == WPM_BINDSTATUS_64BIT_PROGRESS &&
        download_parse_64bit_progress(status_text, &current_64, &total_64)) {
        wpm_progress_set(&callback->progress, current_64, total_64);
        callback->using_64bit_progress = 1;
    } else if (!callback->using_64bit_progress) {
        wpm_progress_set(&callback->progress, current, total);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE download_on_stop_binding(
    download_callback* callback, HRESULT result, LPCWSTR error) {
    (void)callback; (void)result; (void)error; return S_OK;
}

static HRESULT STDMETHODCALLTYPE download_get_bind_info(
    download_callback* callback, DWORD* flags, void* information) {
    (void)callback; (void)flags; (void)information; return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE download_on_data_available(download_callback* callback,
    DWORD flags, DWORD size, void* format, void* medium) {
    (void)callback; (void)flags; (void)size; (void)format; (void)medium; return S_OK;
}

static HRESULT STDMETHODCALLTYPE download_on_object_available(
    download_callback* callback, const GUID* interface_id, void* object) {
    (void)callback; (void)interface_id; (void)object; return S_OK;
}

static const download_callback_vtable download_progress_vtable = {
    download_query_interface,
    download_add_ref,
    download_release,
    download_on_start_binding,
    download_get_priority,
    download_on_low_resource,
    download_on_progress,
    download_on_stop_binding,
    download_get_bind_info,
    download_on_data_available,
    download_on_object_available
};

static int urlmon_download(const char* url, const char* destination,
    const char* label) {
    HRESULT result;
    DWORD move_error = ERROR_SUCCESS;
    char temporary[PATH_SIZE];
    int succeeded;
    download_callback callback;
    if (!is_https_repository(url)) return 0;
    if (snprintf(temporary, sizeof(temporary), "%s.download", destination) < 0) return 0;
    memset(&callback, 0, sizeof(callback));
    callback.vtable = &download_progress_vtable;
    callback.references = 1;
    callback.insecure_http = 0;
    strcpy_s(callback.initial_url, sizeof(callback.initial_url), url);
    wpm_progress_start(&callback.progress,
        "Downloading", "Download", "Downloaded", label, 0);
    DeleteFileA(temporary);
    result = URLDownloadToFileA(NULL, url, temporary, 0, &callback);
    succeeded = SUCCEEDED(result);
    if (succeeded && !MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING)) {
        move_error = GetLastError();
        succeeded = 0;
    }
    wpm_progress_finish(&callback.progress, succeeded);
    if (!succeeded) {
        printf("Error: transport read failed (HRESULT 0x%08lx, filesystem error %lu): %s\n",
            (unsigned long)result, (unsigned long)move_error, url);
        DeleteFileA(temporary);
    }
    return succeeded;
}

typedef void* wpm_hinternet;
typedef wpm_hinternet (WINAPI *internet_open_a_fn)(const char*, DWORD,
    const char*, const char*, DWORD);
typedef wpm_hinternet (WINAPI *internet_open_url_a_fn)(wpm_hinternet,
    const char*, const char*, DWORD, DWORD, ULONG_PTR);
typedef BOOL (WINAPI *internet_read_file_fn)(wpm_hinternet, void*, DWORD, DWORD*);
typedef BOOL (WINAPI *internet_close_handle_fn)(wpm_hinternet);
typedef BOOL (WINAPI *http_query_info_a_fn)(wpm_hinternet, DWORD, void*,
    DWORD*, DWORD*);

#define WPM_INTERNET_FLAG_RELOAD 0x80000000UL
#define WPM_INTERNET_FLAG_NO_CACHE_WRITE 0x04000000UL
#define WPM_INTERNET_FLAG_NO_AUTO_REDIRECT 0x00200000UL
#define WPM_INTERNET_FLAG_NO_UI 0x00000200UL
#define WPM_HTTP_QUERY_STATUS_CODE 19UL
#define WPM_HTTP_QUERY_CONTENT_LENGTH 5UL
#define WPM_HTTP_QUERY_LOCATION 33UL
#define WPM_HTTP_QUERY_FLAG_NUMBER 0x20000000UL

static int http_redirect_url(const char* initial, const char* location,
    char* result, size_t size) {
    size_t origin_length;
    int written;
    if (is_http_repository(location)) {
        return same_web_origin(initial, location) &&
            strcpy_s(result, size, location) == 0;
    }
    if (is_web_repository(location) || location[0] != '/') return 0;
    origin_length = web_origin_length(initial);
    written = snprintf(result, size, "%.*s%s", (int)origin_length,
        initial, location);
    return written > 0 && (size_t)written < size;
}

static int http_download(const char* url, const char* destination,
    const char* label) {
    HMODULE library = LoadLibraryA("wininet.dll");
    internet_open_a_fn open_session = NULL;
    internet_open_url_a_fn open_url = NULL;
    internet_read_file_fn read_file = NULL;
    internet_close_handle_fn close_handle = NULL;
    http_query_info_a_fn query_info = NULL;
    wpm_hinternet session = NULL;
    wpm_hinternet request = NULL;
    char current[PATH_SIZE] = "";
    char temporary[PATH_SIZE] = "";
    char location[PATH_SIZE];
    unsigned char buffer[64 * 1024];
    FILE* output = NULL;
    wpm_progress progress;
    unsigned long long received = 0;
    DWORD status = 0;
    DWORD status_size;
    DWORD content_length = 0;
    DWORD content_length_size;
    DWORD location_size;
    DWORD read;
    int redirect_count;
    int succeeded = 0;
    int progress_started = 0;
    memset(&progress, 0, sizeof(progress));
    if (!library || !is_http_repository(url) ||
        snprintf(temporary, sizeof(temporary), "%s.download", destination) < 0 ||
        strcpy_s(current, sizeof(current), url) != 0) goto cleanup;
    open_session = (internet_open_a_fn)GetProcAddress(library, "InternetOpenA");
    open_url = (internet_open_url_a_fn)GetProcAddress(library, "InternetOpenUrlA");
    read_file = (internet_read_file_fn)GetProcAddress(library, "InternetReadFile");
    close_handle = (internet_close_handle_fn)GetProcAddress(library, "InternetCloseHandle");
    query_info = (http_query_info_a_fn)GetProcAddress(library, "HttpQueryInfoA");
    if (!open_session || !open_url || !read_file || !close_handle || !query_info) goto cleanup;
    session = open_session("WPM/2.0", 0, NULL, NULL, 0);
    if (!session) goto cleanup;
    for (redirect_count = 0; redirect_count <= 5; redirect_count++) {
        request = open_url(session, current, NULL, 0,
            WPM_INTERNET_FLAG_RELOAD | WPM_INTERNET_FLAG_NO_CACHE_WRITE |
                WPM_INTERNET_FLAG_NO_AUTO_REDIRECT | WPM_INTERNET_FLAG_NO_UI,
            0);
        if (!request) goto cleanup;
        status_size = sizeof(status);
        if (!query_info(request, WPM_HTTP_QUERY_STATUS_CODE |
            WPM_HTTP_QUERY_FLAG_NUMBER, &status, &status_size, NULL)) goto cleanup;
        if (status < 300 || status >= 400) break;
        location_size = sizeof(location);
        if (!query_info(request, WPM_HTTP_QUERY_LOCATION, location,
            &location_size, NULL) ||
            !http_redirect_url(url, location, current, sizeof(current))) {
            printf("Error: HTTP redirect changed to a disallowed origin or scheme.\n");
            goto cleanup;
        }
        close_handle(request);
        request = NULL;
    }
    if (!request || status < 200 || status >= 300) goto cleanup;
    content_length_size = sizeof(content_length);
    if (!query_info(request, WPM_HTTP_QUERY_CONTENT_LENGTH |
        WPM_HTTP_QUERY_FLAG_NUMBER, &content_length, &content_length_size, NULL)) {
        content_length = 0;
    }
    DeleteFileA(temporary);
    output = wpm_fopen(temporary, "wb");
    if (!output) goto cleanup;
    wpm_progress_start(&progress, "Downloading", "Download", "Downloaded",
        label, 0);
    progress_started = 1;
    do {
        if (!read_file(request, buffer, sizeof(buffer), &read) ||
            (read && fwrite(buffer, 1, read, output) != read)) goto cleanup;
        received += read;
        wpm_progress_set(&progress, received, 0);
    } while (read);
    if (content_length && received != content_length) {
        printf("Error: HTTP response was incomplete (expected %lu bytes, received %llu).\n",
            (unsigned long)content_length, received);
        goto cleanup;
    }
    if (fclose(output)) { output = NULL; goto cleanup; }
    output = NULL;
    succeeded = MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING);
cleanup:
    if (output) fclose(output);
    if (request && close_handle) close_handle(request);
    if (session && close_handle) close_handle(session);
    if (library) FreeLibrary(library);
    if (progress_started) wpm_progress_finish(&progress, succeeded);
    if (!succeeded) {
        printf("Error: insecure HTTP read failed (status %lu, Windows error %lu): %s\n",
            (unsigned long)status, (unsigned long)GetLastError(), current);
        if (temporary[0]) DeleteFileA(temporary);
    }
    return succeeded;
}
static int copy_local(const char* source, const char* destination) {
    char temporary[PATH_SIZE];
    if (snprintf(temporary, sizeof(temporary), "%s.download", destination) < 0) return 0;
    DeleteFileA(temporary);
    if (!CopyFileA(source, temporary, FALSE)) return 0;
    if (!SetFileAttributesA(temporary, FILE_ATTRIBUTE_NORMAL)) {
        DeleteFileA(temporary);
        return 0;
    }
    if (!MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(temporary);
        return 0;
    }
    return 1;
}
static int retrieve(const char* source, const char* destination, const char* label,
    int allow_insecure_http) {
    if (is_https_repository(source)) return urlmon_download(source, destination, label);
    if (is_http_repository(source)) {
        if (!allow_insecure_http) return 0;
        printf("Warning: using insecure HTTP transport for %s; content remains subject "
            "to package signature and trust validation.\n", source);
        return http_download(source, destination, label);
    }
    REPO_VERBOSE("copying local source: %s", source);
    return copy_local(source, destination);
}
static int index_is_fresh(const char* path) { WIN32_FILE_ATTRIBUTE_DATA data; FILETIME now; ULARGE_INTEGER a, b; GetSystemTimeAsFileTime(&now); if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return 0; a.LowPart = now.dwLowDateTime; a.HighPart = now.dwHighDateTime; b.LowPart = data.ftLastWriteTime.dwLowDateTime; b.HighPart = data.ftLastWriteTime.dwHighDateTime; return a.QuadPart >= b.QuadPart && (a.QuadPart - b.QuadPart) <= INDEX_FRESH_MS * 10000ULL; }
static int index_source(const char* root, char* result, size_t size) { return is_web_repository(root) ? snprintf(result, size, "%s/index.json", root) > 0 : join_path(result, size, root, "index.json"); }
static int refresh(repository* repo, int offline, int required) {
    char cached[PATH_SIZE], url[PATH_SIZE]; if (!cache_path(repo->url, cached, sizeof(cached))) return 0;
    REPO_VERBOSE("checking: %s", repo->url); REPO_VERBOSE("cache: %s", cached);
    if (offline && is_web_repository(repo->url)) { if (GetFileAttributesA(cached) == INVALID_FILE_ATTRIBUTES) printf("Error: no cached index for %s while offline.\n", repo->url); else REPO_VERBOSE("using cached index (offline)"); return GetFileAttributesA(cached) != INVALID_FILE_ATTRIBUTES; }
    if (!required && is_web_repository(repo->url) && index_is_fresh(cached)) { REPO_VERBOSE("using fresh cached index"); return 1; }
    if (ensure_cache_directory("repositories") && index_source(repo->url, url, sizeof(url)) && retrieve(url, cached, "repository index", repo->allow_insecure_http)) { printf("Updated repository index: %s\n", repo->url); return 1; }
    if (!is_web_repository(repo->url)) {
        printf("Error: could not read filesystem repository index: %s\n", repo->url);
        return 0;
    }
    if (GetFileAttributesA(cached) != INVALID_FILE_ATTRIBUTES) { printf("Warning: could not refresh %s; using cached index.\n", repo->url); return 1; }
    printf("Warning: could not retrieve repository index: %s\n", repo->url); return 0;
}
static int report_available_updates(void);
int wpm_repo_update(int offline) { repository repositories[MAX_REPOSITORIES]; int count, i, refreshed = 0; printf("Updating repositories...\n"); if (!load_repositories(repositories, &count)) return 0; if (!count) { printf("No repositories configured.\n"); return 1; } for (i = 0; i < count; i++) if (refresh(&repositories[i], offline, 1)) refreshed = 1; if (!refreshed) return 0; return report_available_updates(); }

static const char* skip_ws(const char* p) { while (*p && isspace((unsigned char)*p)) p++; return p; }
static int json_string(const char** source, char* output, size_t size) { const char* p = skip_ws(*source); size_t n = 0; if (*p++ != '"') return 0; while (*p && *p != '"') { if (*p == '\\') { p++; if (!*p) return 0; } if (n + 1 >= size) return 0; output[n++] = *p++; } if (*p != '"') return 0; output[n] = '\0'; *source = p + 1; return 1; }
static int json_value_end(const char** source) { const char* p = skip_ws(*source); int depth = 0, quote = 0; while (*p) { if (quote) { if (*p == '\\' && p[1]) p += 2; else if (*p++ == '"') quote = 0; else p++; continue; } if (*p == '"') quote = 1; else if (*p == '{' || *p == '[') depth++; else if (*p == '}' || *p == ']') { if (!depth--) break; } else if (*p == ',' && depth == 0) break; p++; } *source = p; return 1; }
static int parse_index(const char* path, repository* repo, package_entry* packages, int* count) {
    FILE* f = wpm_fopen(path, "rb"); long size; char* text; const char* p; int version_ok = 0, before = *count, complete = 1;
    if (!f) return 0; fseek(f, 0, SEEK_END); size = ftell(f); rewind(f); if (size < 2 || size > 4 * 1024 * 1024) { fclose(f); return 0; } text = malloc((size_t)size + 1); if (!text) { fclose(f); return 0; } if (fread(text, 1, (size_t)size, f) != (size_t)size) { free(text); fclose(f); return 0; } fclose(f); text[size] = '\0';
    version_ok = strstr(text, "\"version\":1") != NULL || strstr(text, "\"version\": 1") != NULL;
    p = text; while ((p = strchr(p, '"')) != NULL) { char key[32]; const char* value; if (!json_string(&p, key, sizeof(key))) break; p = skip_ws(p); if (*p++ != ':') continue; value = skip_ws(p); if (strcmp(key, "packages") == 0 && *value == '[') { p = value + 1; break; } json_value_end(&p); }
    if (!version_ok) { printf("Warning: repository index has unsupported schema: %s\n", repo->url); free(text); return 0; }
    while (*p && *p != ']') { char name[128] = "", version[64] = "", arch[16] = "", url[PATH_SIZE] = ""; p = skip_ws(p); if (*p++ != '{') { p++; continue; } while (*p && *p != '}') { char key[32]; p = skip_ws(p); if (!json_string(&p, key, sizeof(key))) break; p = skip_ws(p); if (*p++ != ':') break; if (strcmp(key,"name") == 0) json_string(&p,name,sizeof(name)); else if (strcmp(key,"version") == 0) json_string(&p,version,sizeof(version)); else if (strcmp(key,"arch") == 0) json_string(&p,arch,sizeof(arch)); else if (strcmp(key,"url") == 0) json_string(&p,url,sizeof(url)); else json_value_end(&p); p = skip_ws(p); if (*p == ',') p++; } if (*p == '}') p++; if (name[0] && version[0] && arch[0] && url[0]) { if (*count < MAX_PACKAGES) { package_entry* e = &packages[(*count)++]; strcpy_s(e->name,sizeof(e->name),name); strcpy_s(e->version,sizeof(e->version),version); strcpy_s(e->arch,sizeof(e->arch),arch); strcpy_s(e->url,sizeof(e->url),url); e->priority=repo->priority; e->order=repo->order; } else complete = 0; } p = skip_ws(p); if (*p == ',') p++; }
    REPO_VERBOSE("parsed %d package entries from %s%s", *count - before, repo->url, complete ? "" : " (entry capacity reached)");
    if (!complete) printf("Warning: repository package capacity (%d) was reached while reading %s; later entries or repositories may be unavailable.\n", MAX_PACKAGES, repo->url);
    free(text); return 1;
}
static int safe_relative_item(const char* item) {
    const char* segment;
    const char* cursor;
    size_t length;
    if (!item || !*item || item[0] == '/' || item[0] == '\\' ||
        strchr(item, ':') || strstr(item, "://")) return 0;
    segment = item;
    for (cursor = item; ; cursor++) {
        if (*cursor != '/' && *cursor != '\\' && *cursor != '\0') continue;
        length = (size_t)(cursor - segment);
        if (!length || (length == 1 && segment[0] == '.') ||
            (length == 2 && segment[0] == '.' && segment[1] == '.')) return 0;
        if (!*cursor) break;
        segment = cursor + 1;
    }
    return 1;
}
static int package_source(const char* root, const char* item, char* result, size_t size) {
    int written;
    if (is_web_repository(root)) {
        if (is_https_repository(item)) {
            return is_https_repository(root) && strcpy_s(result, size, item) == 0;
        }
        if (is_http_repository(item)) {
            return is_http_repository(root) && same_web_origin(root, item) &&
                strcpy_s(result, size, item) == 0;
        }
        if (!safe_relative_item(item)) return 0;
        written = snprintf(result, size, "%s/%s", root, item);
        return written > 0 && (size_t)written < size;
    }
    if (!safe_relative_item(item)) return 0;
    return join_path(result, size, root, item);
}

typedef struct { unsigned long long major, minor, patch; const char* prerelease; size_t prerelease_length; } semver;
typedef struct { char name[128]; char version[64]; char arch[16]; } installed_entry;

static int parse_number(const char** p, unsigned long long* value) {
    const char* start = *p; unsigned long long result = 0;
    if (!isdigit((unsigned char)*start)) return 0;
    if (*start == '0' && isdigit((unsigned char)start[1])) return 0;
    while (isdigit((unsigned char)**p)) { unsigned digit = (unsigned)(**p - '0'); if (result > (ULLONG_MAX-digit)/10) return 0; result=result*10+digit; (*p)++; }
    *value=result; return *p > start;
}
static int valid_identifiers(const char* p, const char* end, int prerelease) {
    while (p < end) { const char* start=p; int numeric=1; while(p<end&&*p!='.') { if(!isalnum((unsigned char)*p)&&*p!='-') return 0; if(!isdigit((unsigned char)*p)) numeric=0; p++; } if(p==start||(prerelease&&numeric&&p-start>1&&*start=='0')) return 0; if(p<end)p++; } return 1;
}
static int semver_parse(const char* text, semver* result) {
    const char *p=text,*pre=NULL,*plus=NULL,*end;
    if(!text||!parse_number(&p,&result->major)||*p++!='.'||!parse_number(&p,&result->minor)||*p++!='.'||!parse_number(&p,&result->patch)) return 0;
    if(*p=='-'){ pre=++p; while(*p&&*p!='+')p++; }
    if(*p=='+'){ plus=++p; while(*p)p++; }
    if(*p) return 0; end=plus?plus-1:p;
    if(pre&&(pre==end||!valid_identifiers(pre,end,1))) return 0;
    if(plus&&(plus==p||!valid_identifiers(plus,p,0))) return 0;
    result->prerelease=pre; result->prerelease_length=pre?(size_t)(end-pre):0; return 1;
}
static int identifier_compare(const char* a,size_t an,const char* b,size_t bn){int ad=1,bd=1;size_t i,n;for(i=0;i<an;i++)if(!isdigit((unsigned char)a[i]))ad=0;for(i=0;i<bn;i++)if(!isdigit((unsigned char)b[i]))bd=0;if(ad&&bd){while(an>1&&*a=='0'){a++;an--;}while(bn>1&&*b=='0'){b++;bn--;}if(an!=bn)return an>bn?1:-1;}else if(ad!=bd)return ad?-1:1;n=an<bn?an:bn;for(i=0;i<n;i++)if(a[i]!=b[i])return(unsigned char)a[i]>(unsigned char)b[i]?1:-1;return an==bn?0:(an>bn?1:-1);}
static int semver_compare_parsed(const semver* a,const semver* b){const char *ap,*bp,*ae,*be;if(a->major!=b->major)return a->major>b->major?1:-1;if(a->minor!=b->minor)return a->minor>b->minor?1:-1;if(a->patch!=b->patch)return a->patch>b->patch?1:-1;if(!a->prerelease&&!b->prerelease)return 0;if(!a->prerelease)return 1;if(!b->prerelease)return-1;ap=a->prerelease;bp=b->prerelease;ae=ap+a->prerelease_length;be=bp+b->prerelease_length;while(ap<ae&&bp<be){const char *ax=memchr(ap,'.',(size_t)(ae-ap)),*bx=memchr(bp,'.',(size_t)(be-bp));size_t an=ax?(size_t)(ax-ap):(size_t)(ae-ap),bn=bx?(size_t)(bx-bp):(size_t)(be-bp);int c=identifier_compare(ap,an,bp,bn);if(c)return c;ap+=an+(ax!=NULL);bp+=bn+(bx!=NULL);}return ap==ae?(bp==be?0:-1):1;}
static int semver_compare(const char* a,const char* b,int* valid){semver av,bv;if(!semver_parse(a,&av)||!semver_parse(b,&bv)){if(valid)*valid=0;return 0;}if(valid)*valid=1;return semver_compare_parsed(&av,&bv);}
static int is_prerelease(const char* value){semver parsed;return semver_parse(value,&parsed)&&parsed.prerelease!=NULL;}

static int prerelease_path(char* path,size_t size){char root[PATH_SIZE];return wpm_get_data_root(root,sizeof(root))&&join_path(path,size,root,"config\\prerelease.txt");}
static int prerelease_effective(const char* package,int* overridden){char path[PATH_SIZE],line[512];FILE*f;int global=0,value=0,found=0;if(overridden)*overridden=0;if(!prerelease_path(path,sizeof(path)))return 0;f=wpm_fopen(path,"r");if(!f)return 0;while(fgets(line,sizeof(line),f)){char*tab;line[strcspn(line,"\r\n")]=0;if(_strnicmp(line,"global=",7)==0)global=_stricmp(line+7,"true")==0;else if((tab=strchr(line,'\t'))!=NULL){*tab=0;if(package&&_stricmp(line,package)==0){value=_stricmp(tab+1,"true")==0;found=1;}}}fclose(f);if(found){if(overridden)*overridden=1;return value;}return global;}
static int rewrite_prerelease(const char* package,int enabled,int remove){char path[PATH_SIZE],tmp[PATH_SIZE],line[512];FILE*in,*out;int found=0;if(!prerelease_path(path,sizeof(path))||snprintf(tmp,sizeof(tmp),"%s.tmp",path)<0)return 0;in=wpm_fopen(path,"r");out=wpm_fopen(tmp,"w");if(!out){if(in)fclose(in);return 0;}if(in){while(fgets(line,sizeof(line),in)){char copy[512],*tab;strcpy_s(copy,sizeof(copy),line);line[strcspn(line,"\r\n")]=0;if(!package&&_strnicmp(line,"global=",7)==0){found=1;if(!remove)fprintf(out,"global=%s\n",enabled?"true":"false");}else if(package&&(tab=strchr(line,'\t'))!=NULL){*tab=0;if(_stricmp(line,package)==0){found=1;if(!remove)fprintf(out,"%s\t%s\n",package,enabled?"true":"false");}else fputs(copy,out);}else fputs(copy,out);}fclose(in);}if(!remove&&!found){if(package)fprintf(out,"%s\t%s\n",package,enabled?"true":"false");else fprintf(out,"global=%s\n",enabled?"true":"false");}if(fclose(out)||!MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING)){DeleteFileA(tmp);return 0;}return !remove||found;}
int wpm_config_prerelease_set(const char* package_name,int enabled){if(!rewrite_prerelease(package_name,enabled,0)){printf("Error: could not save prerelease configuration.\n");return 0;}printf("Prerelease %s=%s.\n",package_name?package_name:"global",enabled?"true":"false");return 1;}
int wpm_config_prerelease_get(const char* package_name){int overridden=0,value=prerelease_effective(package_name,&overridden);printf("prerelease=%s (%s)\n",value?"true":"false",package_name?(overridden?"package override":"global setting"):"global setting");return 1;}
int wpm_config_prerelease_unset(const char* package_name){if(!package_name||!rewrite_prerelease(package_name,0,1)){printf("Error: package prerelease override was not found.\n");return 0;}printf("Prerelease override removed: %s\n",package_name);return 1;}

static int load_entries(repository* repositories,int* repository_count,package_entry* entries,int* entry_count,int offline){char cached[PATH_SIZE];int usable=0;*entry_count=0;if(!load_repositories(repositories,repository_count))return 0;for(int i=0;i<*repository_count;i++)if(refresh(&repositories[i],offline,0)&&cache_path(repositories[i].url,cached,sizeof(cached))){if(parse_index(cached,&repositories[i],entries,entry_count))usable=1;}REPO_VERBOSE("resolver loaded %d entries from %d repositories",*entry_count,*repository_count);return usable;}
static int load_installed(installed_entry* result,int* count){char root[PATH_SIZE],store[PATH_SIZE],search[PATH_SIZE],path[PATH_SIZE];WIN32_FIND_DATAA item;HANDLE find;*count=0;if(!wpm_get_data_root(root,sizeof(root))||!join_path(store,sizeof(store),root,"packages")||!join_path(search,sizeof(search),store,"*.zip"))return 0;find=FindFirstFileA(search,&item);if(find==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;do{wpm_package_info info;semver parsed;if(*count>=MAX_INSTALLED||!join_path(path,sizeof(path),store,item.cFileName))continue;if(!wpm_archive_inspect(path,&info)){printf("Warning: unreadable installed package record: %s\n  Path: %s\n",item.cFileName,path);continue;}if(!semver_parse(info.version,&parsed)){printf("Warning: installed package record has non-SemVer version '%s': %s\n  Path: %s\n  This legacy record is ignored for update selection; review the archive before deciding whether it is obsolete.\n",info.version,item.cFileName,path);continue;}installed_entry*e=&result[(*count)++];strcpy_s(e->name,sizeof(e->name),info.name);strcpy_s(e->version,sizeof(e->version),info.version);strcpy_s(e->arch,sizeof(e->arch),info.arch);}while(FindNextFileA(find,&item));FindClose(find);return 1;}
static int better_candidate(package_entry* candidate,package_entry* selected){int valid,c;if(!selected)return 1;c=semver_compare(candidate->version,selected->version,&valid);if(!valid)return 0;return c>0||(c==0&&(candidate->priority>selected->priority||(candidate->priority==selected->priority&&candidate->order<selected->order)));}
static package_entry* select_candidate(package_entry* entries,int count,const char* name,const char* arch,const char* exact_version,int install_mode){package_entry*selected=NULL;const char*available_arch=NULL;int allow_pre=prerelease_effective(name,NULL),named=0,version_matched=0,arch_matched=0,prerelease_excluded=0,architecture_excluded=0;for(int i=0;i<count;i++){semver parsed;if(_stricmp(entries[i].name,name)!=0)continue;named++;if(!semver_parse(entries[i].version,&parsed)){printf("Warning: ignoring invalid SemVer %s for %s.\n",entries[i].version,name);continue;}if(exact_version&&strcmp(entries[i].version,exact_version)!=0)continue;if(is_prerelease(entries[i].version)&&!allow_pre){if((install_mode&&!arch&&(_stricmp(entries[i].arch,WPM_TARGET_ARCH)==0||_stricmp(entries[i].arch,"any")==0))||((!install_mode||arch)&&_stricmp(entries[i].arch,arch)==0))prerelease_excluded++;continue;}version_matched++;if(install_mode&&!arch){if(_stricmp(entries[i].arch,WPM_TARGET_ARCH)!=0&&_stricmp(entries[i].arch,"any")!=0){architecture_excluded++;if(!available_arch)available_arch=entries[i].arch;continue;}if(selected&&_stricmp(selected->arch,WPM_TARGET_ARCH)==0&&_stricmp(entries[i].arch,"any")==0)continue;if(selected&&_stricmp(selected->arch,"any")==0&&_stricmp(entries[i].arch,WPM_TARGET_ARCH)==0){selected=&entries[i];arch_matched++;continue;}}else if(_stricmp(entries[i].arch,arch)!=0){architecture_excluded++;if(!available_arch)available_arch=entries[i].arch;continue;}arch_matched++;if(better_candidate(&entries[i],selected))selected=&entries[i];}REPO_VERBOSE("resolution for '%s': name matches=%d, version/prerelease matches=%d, architecture matches=%d, prereleases excluded=%d, architectures excluded=%d, selected=%s",name,named,version_matched,arch_matched,prerelease_excluded,architecture_excluded,selected?selected->version:"none");if(install_mode&&!selected&&prerelease_excluded)printf("Warning: matching prerelease packages were excluded because prereleases are disabled.\n  Enable them for this package with: wpm config set prerelease true --package %s\n",name);else if(install_mode&&!selected&&architecture_excluded)printf("Warning: matching packages were found, but none support architecture %s.\n  An available architecture is: %s\n  Retry with: wpm install %s --arch %s\n",arch?arch:WPM_TARGET_ARCH,available_arch,name,available_arch);return selected;}
static int obtain(repository* repositories,package_entry* selected,int offline,
    char* path,size_t size) {
    repository* repo = &repositories[selected->order];
    char root[PATH_SIZE], source[PATH_SIZE], legacy[PATH_SIZE], label[256];
    if (strpbrk(selected->name,"\\/:*") || strpbrk(selected->version,"\\/:*") ||
        strpbrk(selected->arch,"\\/:*") ||
        !package_source(repo->url,selected->url,source,sizeof(source)) ||
        !wpm_get_data_root(root,sizeof(root)) ||
        snprintf(path,size,"%s\\cache\\packages\\%s-%s-%s.zip",root,
            selected->name,selected->arch,selected->version) < 0 ||
        snprintf(legacy,sizeof(legacy),"%s\\cache\\packages\\%s-%s-%s.zip",root,
            selected->name,selected->version,selected->arch) < 0 ||
        snprintf(label,sizeof(label),"package %s %s %s",selected->name,
            selected->version,selected->arch) < 0) return 0;
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesA(legacy) != INVALID_FILE_ATTRIBUTES) strcpy_s(path,size,legacy);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        if (offline && is_web_repository(repo->url)) {
            printf("Error: package is not cached while offline: %s\n",selected->name);
            return 0;
        }
        if (!ensure_cache_directory("packages") ||
            !retrieve(source,path,label,repo->allow_insecure_http)) {
            printf("Error: could not retrieve package: %s\n",source);
            return 0;
        }
    }
    return 1;
}

int wpm_repo_install(const char* package_name,const char* arch,const char* version,int offline,int allow_unsigned){repository repositories[MAX_REPOSITORIES];package_entry entries[MAX_PACKAGES],*selected;int rc,ec,result;char package[PATH_SIZE];semver parsed;if(version&&!semver_parse(version,&parsed)){printf("Error: --version requires valid SemVer.\n");return 0;}if(arch&&_stricmp(arch,"any")&&_stricmp(arch,"x86")&&_stricmp(arch,"x64")&&_stricmp(arch,"arm64")){printf("Error: unsupported architecture: %s\n",arch);return 0;}if(!load_entries(repositories,&rc,entries,&ec,offline))return 0;selected=select_candidate(entries,ec,package_name,arch,version,1);if(!selected){printf("Error: package was not found with the requested selectors: %s\n",package_name);return 0;}if(!obtain(repositories,selected,offline,package,sizeof(package))){printf("Error: invalid package URL in repository index.\n");return 0;}printf("Installing %s %s %s from %s\n",selected->name,selected->arch,selected->version,repositories[selected->order].url);wpm_archive_set_repository_url(repositories[selected->order].url);result=wpm_archive_install(package,allow_unsigned);wpm_archive_set_repository_url(NULL);return result;}

static int requested(const char* name,const char** names,int count){for(int i=0;i<count;i++)if(_stricmp(name,names[i])==0)return 1;return 0;}
static int installed_is_latest(installed_entry* installed,int count,int index){int valid;for(int j=0;j<count;j++)if(index!=j&&_stricmp(installed[index].name,installed[j].name)==0&&_stricmp(installed[index].arch,installed[j].arch)==0&&semver_compare(installed[j].version,installed[index].version,&valid)>0)return 0;return 1;}
static int conflicting_any(installed_entry* installed,int count,int index){if(_stricmp(installed[index].arch,"any")!=0)return 0;for(int j=0;j<count;j++)if(index!=j&&_stricmp(installed[index].name,installed[j].name)==0&&_stricmp(installed[j].arch,"any")!=0)return 1;return 0;}
static package_entry* newer_candidate(package_entry* entries,int entry_count,installed_entry* item,const char* exact_version){package_entry*selected=select_candidate(entries,entry_count,item->name,item->arch,exact_version,0);int valid;if(!selected||semver_compare(selected->version,item->version,&valid)<=0||!valid)return NULL;return selected;}
static int print_upgrade_plan(installed_entry* installed,int installed_count,package_entry* entries,int entry_count,const char* arch){int count=0;for(int i=0;i<installed_count;i++){package_entry*selected;if((arch&&_stricmp(arch,installed[i].arch)!=0)||!installed_is_latest(installed,installed_count,i)||conflicting_any(installed,installed_count,i))continue;selected=newer_candidate(entries,entry_count,&installed[i],NULL);if(!selected)continue;if(!count)printf("Planned upgrades:\n");printf("  %s %s: %s -> %s\n",installed[i].name,installed[i].arch,installed[i].version,selected->version);count++;}return count;}
static int confirm_upgrade_plan(int assume_yes){
    char answer[16];
    HANDLE input;
    DWORD read = 0;
    size_t end;
    if(assume_yes)return 1;
    printf("Proceed with these upgrades? [y/N]: ");
    fflush(stdout);
    input=GetStdHandle(STD_INPUT_HANDLE);
    if(input==NULL||input==INVALID_HANDLE_VALUE||
        !ReadFile(input,answer,sizeof(answer)-1,&read,NULL)||read==0)return 0;
    answer[read]='\0';
    end=strcspn(answer,"\r\n");
    answer[end]='\0';
    return _stricmp(answer,"y")==0||_stricmp(answer,"yes")==0;
}

static int count_upgrade_candidates(installed_entry* installed,int installed_count,package_entry* entries,int entry_count,const char** package_names,int package_count,int all,const char* arch,const char* version){int count=0;for(int i=0;i<installed_count;i++){package_entry*selected;int valid;if(!all&&!requested(installed[i].name,package_names,package_count))continue;if(arch&&_stricmp(arch,installed[i].arch)!=0)continue;if(!installed_is_latest(installed,installed_count,i)||conflicting_any(installed,installed_count,i))continue;selected=select_candidate(entries,entry_count,installed[i].name,installed[i].arch,version,0);if(selected&&semver_compare(selected->version,installed[i].version,&valid)>0&&valid)count++;}return count;}

static int report_available_updates(void){repository repositories[MAX_REPOSITORIES];package_entry entries[MAX_PACKAGES];installed_entry installed[MAX_INSTALLED];int rc,ec,ic,count;if(!load_installed(installed,&ic)||!load_entries(repositories,&rc,entries,&ec,1))return 0;count=print_upgrade_plan(installed,ic,entries,ec,NULL);if(!count)printf("All installed packages are current.\n");else printf("%d package %s can be upgraded. Run 'wpm upgrade --all' to review and apply.\n",count,count==1?"identity":"identities");return 1;}

int wpm_repo_upgrade(const char** package_names,int package_count,int all,const char* arch,const char* version,int offline,int allow_unsigned,int assume_yes){repository repositories[MAX_REPOSITORIES];package_entry entries[MAX_PACKAGES];installed_entry installed[MAX_INSTALLED];int rc,ec,ic,failures=0,processed=0,progress_current=0,progress_total;semver parsed;if(version&&!semver_parse(version,&parsed)){printf("Error: --version requires valid SemVer.\n");return 0;}if(!load_installed(installed,&ic)||!load_entries(repositories,&rc,entries,&ec,offline))return 0;progress_total=count_upgrade_candidates(installed,ic,entries,ec,package_names,package_count,all,arch,version);if(all){int planned=print_upgrade_plan(installed,ic,entries,ec,arch);if(planned&&!confirm_upgrade_plan(assume_yes)){printf("Upgrade cancelled; no packages were changed.\n");return 1;}if(planned)printf("\n");}for(int i=0;i<ic;i++){package_entry*selected;int valid,comparison,operation_ok;char cached[PATH_SIZE];int current=1,conflict=0,is_self;if(!all&&!requested(installed[i].name,package_names,package_count))continue;if(arch&&_stricmp(arch,installed[i].arch)!=0)continue;current=installed_is_latest(installed,ic,i);conflict=conflicting_any(installed,ic,i);if(!current)continue;if(conflict){printf("Warning: ignoring conflicting any installation for %s.\n",installed[i].name);if(arch&&_stricmp(arch,"any")==0)failures++;continue;}processed++;selected=select_candidate(entries,ec,installed[i].name,installed[i].arch,version,0);if(!selected){printf("%s %s %s is current.\n",installed[i].name,installed[i].arch,installed[i].version);printf("Result: %s %s current\n",installed[i].name,installed[i].arch);continue;}comparison=semver_compare(selected->version,installed[i].version,&valid);if(!valid||comparison<=0){if(version){printf("Error: requested upgrade version must be newer than installed %s.\n",installed[i].version);printf("Result: %s %s failed\n",installed[i].name,installed[i].arch);failures++;}else{printf("%s %s %s is current.\n",installed[i].name,installed[i].arch,installed[i].version);printf("Result: %s %s current\n",installed[i].name,installed[i].arch);}continue;}is_self=_stricmp(selected->name,"wpm")==0;wpm_archive_set_progress(++progress_current,progress_total);if(!obtain(repositories,selected,offline,cached,sizeof(cached))){printf("Result: %s %s failed\n",installed[i].name,installed[i].arch);failures++;continue;}wpm_archive_set_repository_url(repositories[selected->order].url);operation_ok=is_self?wpm_archive_schedule_self_upgrade(cached,allow_unsigned,selected->version,selected->arch,installed[i].version):wpm_archive_upgrade(cached,allow_unsigned,selected->name,selected->version,selected->arch,installed[i].version);wpm_archive_set_repository_url(NULL);if(!operation_ok){printf("Result: %s %s failed\n",installed[i].name,installed[i].arch);failures++;continue;}printf("Result: %s %s %s\n",installed[i].name,installed[i].arch,is_self?"scheduled":"upgraded");}
for(int n=0;n<package_count;n++){int found=0;for(int i=0;i<ic;i++)if(_stricmp(package_names[n],installed[i].name)==0&&(!arch||_stricmp(arch,installed[i].arch)==0))found=1;if(!found){printf("Error: requested package identity is not installed: %s%s%s\n",package_names[n],arch?" ":"",arch?arch:"");failures++;}}
if(!processed&&!failures){printf("No installed packages matched the request.\n");return 0;}return failures==0;}
