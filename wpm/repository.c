/** @file repository.c @brief Repository and package resolution operations. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <windows.h>
#ifdef __TINYC__
#include "tcc_support/urlmon.h"
#else
#include <urlmon.h>
#endif
#include "archive.h"
#include "helpers.h"
#include "https.h"
#include "repository.h"
#include "logging.h"
#include "capability.h"
#include "operation.h"
#include "progress.h"

#define PATH_SIZE 4096
#define MAX_REPOSITORIES 32
#define MAX_PACKAGES 96
#define MAX_INSTALLED 256
#define INDEX_FRESH_MS (60ULL * 60ULL * 1000ULL)
/* URLMon status omitted by TinyCC's compact Windows headers. */
#define WPM_BINDSTATUS_64BIT_PROGRESS 56UL
#define WPM_BINDSTATUS_REDIRECTING 3UL
#define WPM_DEFAULT_REPOSITORY                                                 \
    "https://github.com/Thewafflication/wpm/releases/latest/download"

typedef struct {
    char url[PATH_SIZE];
    int priority;
    int order;
    int allow_insecure_http;
} repository;
typedef struct {
    char name[128];
    char version[64];
    char arch[16];
    char url[PATH_SIZE];
    int priority;
    int order;
} package_entry;

typedef struct download_callback download_callback;
typedef struct {
    HRESULT(STDMETHODCALLTYPE *query_interface)(download_callback *,
                                                const GUID *, void **);
    ULONG(STDMETHODCALLTYPE *add_ref)(download_callback *);
    ULONG(STDMETHODCALLTYPE *release)(download_callback *);
    HRESULT(STDMETHODCALLTYPE *on_start_binding)(download_callback *, DWORD,
                                                 void *);
    HRESULT(STDMETHODCALLTYPE *get_priority)(download_callback *, LONG *);
    HRESULT(STDMETHODCALLTYPE *on_low_resource)(download_callback *, DWORD);
    HRESULT(STDMETHODCALLTYPE *on_progress)(download_callback *, ULONG, ULONG,
                                            ULONG, LPCWSTR);
    HRESULT(STDMETHODCALLTYPE *on_stop_binding)(download_callback *, HRESULT,
                                                LPCWSTR);
    HRESULT(STDMETHODCALLTYPE *get_bind_info)(download_callback *, DWORD *,
                                              void *);
    HRESULT(STDMETHODCALLTYPE *on_data_available)(download_callback *, DWORD,
                                                  DWORD, void *, void *);
    HRESULT(STDMETHODCALLTYPE *on_object_available)(download_callback *,
                                                    const GUID *, void *);
} download_callback_vtable;

struct download_callback {
    const download_callback_vtable *vtable;
    ULONG references;
    wpm_progress progress;
    int using_64bit_progress;
    int insecure_http;
    char initial_url[PATH_SIZE];
};

static int repository_verbose = 0;

void wpm_repo_set_verbose(int enabled) { repository_verbose = enabled != 0; }

static void repository_verbose_log(const char *format, ...) {
    va_list arguments;
    char message[8192];
    if (!repository_verbose) {
        return;
    }
    va_start(arguments, format);
    if (vsnprintf(message, sizeof(message), format, arguments) >= 0) {
        message[sizeof(message) - 1] = '\0';
        wpm_event_printf(WPM_EVENT_DEBUG, "repository.debug", "repository",
                         "command", "Verbose: Repository: %s\n", message);
    }
    va_end(arguments);
}

#define REPO_VERBOSE(...) repository_verbose_log(__VA_ARGS__)

static int join_path(char *result, size_t size, const char *left,
                     const char *right) {
    int n = snprintf(result, size, "%s%s%s", left,
                     left[strlen(left) - 1] == '\\' ? "" : "\\", right);
    return n >= 0 && (size_t)n < size;
}
static int ensure_directory(const char *path) {
    if (!wpm_require_capability(WPM_CAP_WRITE, "ensure_directory")) {
        return 0;
    }
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY)) ||
           CreateDirectoryA(path, NULL) ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}
static int ensure_cache_directory(const char *leaf) {
    char root[PATH_SIZE], cache[PATH_SIZE], path[PATH_SIZE];
    return wpm_get_data_root(root, sizeof(root)) &&
           join_path(cache, sizeof(cache), root, "cache") &&
           join_path(path, sizeof(path), cache, leaf) && ensure_directory(path);
}
static int config_path(char *path, size_t size) {
    char root[PATH_SIZE];
    return wpm_get_data_root(root, sizeof(root)) &&
           join_path(path, size, root, "config\\repositories.txt");
}
static int cache_path(const char *url, char *path, size_t size) {
    unsigned long hash = 2166136261u;
    char root[PATH_SIZE];
    const unsigned char *p = (const unsigned char *)url;
    while (*p) {
        hash ^= *p++;
        hash *= 16777619u;
    }
    return wpm_get_data_root(root, sizeof(root)) &&
           snprintf(path, size, "%s\\cache\\repositories\\%08lx.json", root,
                    hash) > 0;
}
static int is_https_repository(const char *source) {
    return source && _strnicmp(source, "https://", 8) == 0;
}
static int is_http_repository(const char *source) {
    return source && _strnicmp(source, "http://", 7) == 0;
}
static int is_web_repository(const char *source) {
    return is_https_repository(source) || is_http_repository(source);
}
static size_t web_origin_length(const char *source) {
    const char *authority;
    const char *end;
    size_t scheme_length;
    if (is_https_repository(source)) {
        scheme_length = 8;
    } else if (is_http_repository(source)) {
        scheme_length = 7;
    } else {
        return 0;
    }
    authority = source + scheme_length;
    end = strchr(authority, '/');
    return end ? (size_t)(end - source) : strlen(source);
}
static int same_web_origin(const char *left, const char *right) {
    size_t left_length = web_origin_length(left);
    size_t right_length = web_origin_length(right);
    return left_length && left_length == right_length &&
           _strnicmp(left, right, left_length) == 0;
}
static int web_locator_valid(const char *source, size_t scheme_length) {
    const char *authority = source + scheme_length;
    const char *end = authority;
    if (!*authority || strchr(authority, '?') || strchr(authority, '#')) {
        return 0;
    }
    while (*end && *end != '/') {
        if (*end == '\\' || *end == '@' || *end == '?' || *end == '#') {
            return 0;
        }
        end++;
    }
    return end > authority;
}
static int unc_locator_valid(const char *source) {
    const char *server;
    const char *share;
    const char *end;
    if (!source || source[0] != '\\' || source[1] != '\\' || source[2] == '.' ||
        source[2] == '?' || source[2] == '\\') {
        return 0;
    }
    server = source + 2;
    share = strchr(server, '\\');
    if (!share || share == server || !share[1]) {
        return 0;
    }
    end = strchr(share + 1, '\\');
    if (end == share + 1) {
        return 0;
    }
    return 1;
}
static int repository_normalize(const char *source, char *result, size_t size,
                                int report_error) {
    size_t length;
    DWORD required;
    if (!source || !*source || strpbrk(source, "\t\r\n")) {
        if (report_error) {
            wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                             "command",
                             "Error: repository locator is empty or contains a "
                             "control character.\n");
        }
        return 0;
    }
    if (is_web_repository(source)) {
        if (strpbrk(source, " \t\r\n") || strcpy_s(result, size, source) != 0) {
            return 0;
        }
        if (!web_locator_valid(result, is_https_repository(result) ? 8 : 7)) {
            if (report_error) {
                wpm_event_printf(
                    WPM_EVENT_ERROR, "repository.error", "repository",
                    "command",
                    "Error: repository URL requires an authority and "
                    "cannot contain credentials.\n");
            }
            return 0;
        }
        length = strlen(result);
        while (length > (is_https_repository(result) ? 8u : 7u) &&
               result[length - 1] == '/') {
            result[--length] = '\0';
        }
        return 1;
    }
    if (strstr(source, "://")) {
        if (report_error) {
            wpm_event_printf(
                WPM_EVENT_ERROR, "repository.error", "repository", "command",
                "Error: repository locators must use https://, opted-in "
                "http://, or a filesystem path.\n");
        }
        return 0;
    }
    required = GetFullPathNameA(source, (DWORD)size, result, NULL);
    if (!required || required >= size) {
        if (report_error) {
            wpm_event_printf(
                WPM_EVENT_ERROR, "repository.error", "repository", "command",
                "Error: repository path is invalid or too long: %s\n", source);
        }
        return 0;
    }
    for (length = 0; result[length]; length++) {
        if (result[length] == '/') {
            result[length] = '\\';
        }
    }
    if (result[0] == '\\' && result[1] == '\\') {
        if (!unc_locator_valid(result)) {
            if (report_error) {
                wpm_event_printf(
                    WPM_EVENT_ERROR, "repository.error", "repository",
                    "command",
                    "Error: UNC repositories require a server and share "
                    "and cannot use a device namespace.\n");
            }
            return 0;
        }
        while (length > 2 && result[length - 1] == '\\') {
            result[--length] = '\0';
        }
        return 1;
    }
    if (length < 3 || !isalpha((unsigned char)result[0]) || result[1] != ':' ||
        result[2] != '\\') {
        if (report_error) {
            wpm_event_printf(
                WPM_EVENT_ERROR, "repository.error", "repository", "command",
                "Error: filesystem repositories require a drive-qualified "
                "or UNC path.\n");
        }
        return 0;
    }
    if (strchr(result + 2, ':')) {
        if (report_error) {
            wpm_event_printf(
                WPM_EVENT_ERROR, "repository.error", "repository", "command",
                "Error: filesystem repository paths cannot contain a "
                "colon after the drive prefix.\n");
        }
        return 0;
    }
    while (length > 3 && result[length - 1] == '\\') {
        result[--length] = '\0';
    }
    return 1;
}
static int parse_entry(char *line, int *priority, char **url, int *allow_http) {
    char *tab = strchr(line, '\t');
    char *option;
    char *end;
    if (!tab) {
        return 0;
    }
    *tab = '\0';
    *priority = (int)strtol(line, &end, 10);
    if (*end) {
        return 0;
    }
    *url = tab + 1;
    (*url)[strcspn(*url, "\r\n")] = '\0';
    option = strchr(*url, '\t');
    *allow_http = 0;
    if (option) {
        *option++ = '\0';
        if (strcmp(option, "allow-insecure-http=true") != 0) {
            return 0;
        }
        *allow_http = 1;
    }
    return **url != '\0' && (!is_http_repository(*url) || *allow_http);
}
static int load_repositories(repository *result, int *count,
                             int *invalid_count) {
    char path[PATH_SIZE], line[PATH_SIZE + 32], normalized[PATH_SIZE];
    FILE *input;
    *count = 0;
    if (invalid_count) {
        *invalid_count = 0;
    }
    if (!config_path(path, sizeof(path))) {
        return 0;
    }
    REPO_VERBOSE("configuration: %s", path);
    input = wpm_fopen(path, "r");
    if (input) {
        while (*count < MAX_REPOSITORIES && fgets(line, sizeof(line), input)) {
            char *url;
            int priority, allow_http;
            if (parse_entry(line, &priority, &url, &allow_http)) {
                if (!repository_normalize(url, normalized, sizeof(normalized),
                                          0) ||
                    (!is_web_repository(url) &&
                     _stricmp(url, normalized) != 0)) {
                    if (invalid_count) {
                        (*invalid_count)++;
                        wpm_event_printf(WPM_EVENT_WARNING,
                                         "repository.config.invalid",
                                         "repository", "diagnose",
                                         "Warning: invalid configured "
                                         "repository entry ignored.\n");
                    } else if (!is_web_repository(url)) {
                        wpm_event_printf(
                            WPM_EVENT_WARNING, "repository.warning",
                            "repository", "command",
                            "Warning: ignoring invalid configured filesystem "
                            "repository locator: %s\n"
                            "  Remove it with: wpm repo remove \"%s\"\n",
                            url, url);
                    } else {
                        wpm_event_printf(
                            WPM_EVENT_WARNING, "repository.warning",
                            "repository", "command",
                            "Warning: ignoring an invalid configured web "
                            "repository locator. Remove or re-add it with "
                            "'wpm repo'.\n");
                    }
                    continue;
                }
                {
                    repository *r = &result[*count];
                    r->priority = priority;
                    r->order = *count;
                    r->allow_insecure_http = allow_http;
                    strcpy_s(r->url, sizeof(r->url), normalized);
                    REPO_VERBOSE(
                        "configured[%d]: priority=%d transport=%s url=%s",
                        *count, priority,
                        allow_http ? "insecure-http" : "standard", normalized);
                    (*count)++;
                }
            } else if (invalid_count) {
                char *start = line;
                while (*start && isspace((unsigned char)*start)) {
                    start++;
                }
                if (*start && *start != '#') {
                    (*invalid_count)++;
                }
            }
        }
        fclose(input);
    } else if (invalid_count &&
               GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        (*invalid_count)++;
    }
    for (int i = 0; i < *count; i++) {
        if (_stricmp(result[i].url, WPM_DEFAULT_REPOSITORY) == 0) {
            REPO_VERBOSE("loaded %d repositories", *count);
            return 1;
        }
    }
    if (*count < MAX_REPOSITORIES) {
        repository *r = &result[*count];
        r->priority = 0;
        r->order = *count;
        r->allow_insecure_http = 0;
        strcpy_s(r->url, sizeof(r->url), WPM_DEFAULT_REPOSITORY);
        REPO_VERBOSE("built-in[%d]: priority=0 url=%s", *count,
                     WPM_DEFAULT_REPOSITORY);
        (*count)++;
    }
    REPO_VERBOSE("loaded %d repositories", *count);
    return 1;
}
static int write_entry(FILE *output, int priority, const char *url,
                       int allow_http) {
    return fprintf(output, "%d\t%s%s\n", priority, url,
                   allow_http ? "\tallow-insecure-http=true" : "") >= 0;
}
static int rewrite(const char *wanted, int priority, int allow_http,
                   int remove) {
    if (!wpm_require_capability(WPM_CAP_WRITE, "rewrite")) {
        return 0;
    }
    char path[PATH_SIZE], temporary[PATH_SIZE], line[PATH_SIZE + 32];
    FILE *input, *output;
    int found = 0;
    if (!config_path(path, sizeof(path)) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp", path) < 0) {
        return 0;
    }
    input = wpm_fopen(path, "r");
    output = wpm_fopen(temporary, "w");
    if (!output) {
        if (input) {
            fclose(input);
        }
        wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                         "command",
                         "Error: could not write repository configuration.\n");
        return 0;
    }
    if (input) {
        while (fgets(line, sizeof(line), input)) {
            char original[PATH_SIZE + 32], *url;
            int old_priority, old_allow_http;
            strcpy_s(original, sizeof(original), line);
            if (parse_entry(line, &old_priority, &url, &old_allow_http) &&
                _stricmp(url, wanted) == 0) {
                found = 1;
                if (!remove) {
                    write_entry(output, priority, wanted, allow_http);
                }
            } else {
                fputs(original, output);
            }
        }
        fclose(input);
    }
    if (!remove && !found) {
        write_entry(output, priority, wanted, allow_http);
    }
    if (fclose(output) ||
        !MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(temporary);
        wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                         "command",
                         "Error: could not save repository configuration.\n");
        return 0;
    }
    if (remove && !found) {
        wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                         "command", "Error: repository is not configured: %s\n",
                         wanted);
        return 0;
    }
    wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository", "command",
                     "Repository %s: %s\n",
                     remove ? "removed" : (found ? "updated" : "added"),
                     wanted);
    return 1;
}
int wpm_repo_add(const char *url, int priority, int allow_insecure_http) {
    char normalized[PATH_SIZE];
    if (!repository_normalize(url, normalized, sizeof(normalized), 1)) {
        return 0;
    }
    if (is_http_repository(normalized) && !allow_insecure_http) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.error", "repository", "command",
            "Error: plain HTTP repositories require --allow-insecure-http.\n");
        return 0;
    }
    if (!is_http_repository(normalized) && allow_insecure_http) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.error", "repository", "command",
            "Error: --allow-insecure-http is valid only for an http:// "
            "repository.\n");
        return 0;
    }
    if (allow_insecure_http) {
        wpm_event_printf(
            WPM_EVENT_WARNING, "repository.warning", "repository", "command",
            "Warning: insecure HTTP transport exposes repository metadata "
            "and downloads; "
            "package signatures and trust checks remain required.\n");
    }
    if (!rewrite(normalized, priority, allow_insecure_http, 0)) {
        return 0;
    }
    wpm_event_printf(WPM_EVENT_RESULT, "repository.result", "repository",
                     "command", "Result: repository configured %s\n",
                     normalized);
    return 1;
}
int wpm_repo_remove(const char *url) {
    char normalized[PATH_SIZE], cached[PATH_SIZE];
    const char *wanted;
    if (repository_normalize(url, normalized, sizeof(normalized), 0)) {
        wanted = normalized;
    } else {
        if (!url || !*url || strstr(url, "://") || strpbrk(url, "\t\r\n")) {
            repository_normalize(url, normalized, sizeof(normalized), 1);
            return 0;
        }
        wanted = url;
    }
    if (!rewrite(wanted, 0, 0, 1)) {
        return 0;
    }
    if (cache_path(wanted, cached, sizeof(cached))) {
        DeleteFileA(cached);
    }
    wpm_event_printf(WPM_EVENT_RESULT, "repository.result", "repository",
                     "command", "Result: repository removed %s\n", wanted);
    return 1;
}
int wpm_repo_list(void) {
    repository repositories[MAX_REPOSITORIES];
    int count, i;
    if (!load_repositories(repositories, &count, NULL)) {
        return 0;
    }
    if (!count) {
        wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                         "command", "No repositories configured.\n");
        return 1;
    }
    for (i = 0; i < count; i++) {
        wpm_event_printf(
            WPM_EVENT_INFO, "repository.info", "repository", "command",
            "%d\t%s%s\n", repositories[i].priority, repositories[i].url,
            repositories[i].allow_insecure_http ? "\t[insecure HTTP allowed]"
                                                : "");
    }
    return 1;
}

static int download_guid_equal(const GUID *left, const GUID *right) {
    return left != NULL && right != NULL &&
           memcmp(left, right, sizeof(*left)) == 0;
}

static HRESULT STDMETHODCALLTYPE download_query_interface(
    download_callback *callback, const GUID *interface_id, void **result) {
    static const GUID unknown_id = {
        0x00000000UL,
        0x0000,
        0x0000,
        {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
    static const GUID bind_status_callback_id = {
        0x79eac9c1UL,
        0xbaf9,
        0x11ce,
        {0x8c, 0x82, 0x00, 0xaa, 0x00, 0x4b, 0xa9, 0x0b}};
    if (!result) {
        return E_POINTER;
    }
    *result = NULL;
    if (!download_guid_equal(interface_id, &unknown_id) &&
        !download_guid_equal(interface_id, &bind_status_callback_id)) {
        return E_NOINTERFACE;
    }
    *result = callback;
    callback->references++;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE download_add_ref(download_callback *callback) {
    return ++callback->references;
}

static ULONG STDMETHODCALLTYPE download_release(download_callback *callback) {
    if (callback->references) {
        callback->references--;
    }
    return callback->references;
}

static HRESULT STDMETHODCALLTYPE download_on_start_binding(
    download_callback *callback, DWORD reserved, void *binding) {
    (void)callback;
    (void)reserved;
    (void)binding;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
download_get_priority(download_callback *callback, LONG *priority) {
    (void)callback;
    (void)priority;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
download_on_low_resource(download_callback *callback, DWORD reserved) {
    (void)callback;
    (void)reserved;
    return S_OK;
}

static int download_parse_64bit_progress(const wchar_t *text,
                                         unsigned long long *current,
                                         unsigned long long *total) {
    const wchar_t *cursor = text;
    unsigned long long values[2] = {0, 0};
    int value_index;
    if (!text || !current || !total) {
        return 0;
    }
    for (value_index = 0; value_index < 2; value_index++) {
        const wchar_t *start = cursor;
        while (*cursor >= L'0' && *cursor <= L'9') {
            unsigned digit = (unsigned)(*cursor - L'0');
            if (values[value_index] > (ULLONG_MAX - digit) / 10ULL) {
                return 0;
            }
            values[value_index] = values[value_index] * 10ULL + digit;
            cursor++;
        }
        if (cursor == start) {
            return 0;
        }
        if (value_index == 0) {
            if (*cursor++ != L',') {
                return 0;
            }
        } else if (*cursor != L'\0') {
            return 0;
        }
    }
    *current = values[0];
    *total = values[1];
    return 1;
}

static HRESULT STDMETHODCALLTYPE
download_on_progress(download_callback *callback, ULONG current, ULONG total,
                     ULONG status, LPCWSTR status_text) {
    unsigned long long current_64;
    unsigned long long total_64;
    if (status == WPM_BINDSTATUS_REDIRECTING && status_text) {
        char redirect[PATH_SIZE];
        int converted =
            WideCharToMultiByte(CP_UTF8, 0, status_text, -1, redirect,
                                sizeof(redirect), NULL, NULL);
        if (!converted || !is_web_repository(redirect) ||
            (callback->insecure_http &&
             (!is_http_repository(redirect) ||
              !same_web_origin(callback->initial_url, redirect))) ||
            (!callback->insecure_http && is_http_repository(redirect))) {
            wpm_event_printf(
                WPM_EVENT_ERROR, "repository.error", "repository", "command",
                "Error: repository redirect changed to a disallowed origin "
                "or scheme.\n");
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
    download_callback *callback, HRESULT result, LPCWSTR error) {
    (void)callback;
    (void)result;
    (void)error;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE download_get_bind_info(
    download_callback *callback, DWORD *flags, void *information) {
    (void)callback;
    (void)flags;
    (void)information;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
download_on_data_available(download_callback *callback, DWORD flags, DWORD size,
                           void *format, void *medium) {
    (void)callback;
    (void)flags;
    (void)size;
    (void)format;
    (void)medium;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE download_on_object_available(
    download_callback *callback, const GUID *interface_id, void *object) {
    (void)callback;
    (void)interface_id;
    (void)object;
    return S_OK;
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
    download_on_object_available};

static int urlmon_download(const char *url, const char *destination,
                           const char *label) {
    if (!wpm_require_capability(WPM_CAP_WRITE | WPM_CAP_NETWORK,
                                "urlmon_download")) {
        return 0;
    }
    HRESULT result;
    DWORD move_error = ERROR_SUCCESS;
    char temporary[PATH_SIZE];
    int succeeded;
    download_callback callback;
    if (!is_https_repository(url)) {
        return 0;
    }
    if (snprintf(temporary, sizeof(temporary), "%s.download", destination) <
        0) {
        return 0;
    }
    memset(&callback, 0, sizeof(callback));
    callback.vtable = &download_progress_vtable;
    callback.references = 1;
    callback.insecure_http = 0;
    strcpy_s(callback.initial_url, sizeof(callback.initial_url), url);
    wpm_progress_start(&callback.progress, "Downloading", "Download",
                       "Downloaded", label, 0);
    DeleteFileA(temporary);
    result = URLDownloadToFileA(NULL, url, temporary, 0, &callback);
    succeeded = SUCCEEDED(result);
    if (succeeded) {
        WIN32_FILE_ATTRIBUTE_DATA information;
        /* Cached URLMon responses need not deliver byte callbacks. */
        if (GetFileAttributesExA(temporary, GetFileExInfoStandard,
                                 &information)) {
            unsigned long long size =
                ((unsigned long long)information.nFileSizeHigh << 32) |
                information.nFileSizeLow;
            wpm_progress_set(&callback.progress, size, size);
        }
    }
    if (succeeded &&
        !MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING)) {
        move_error = GetLastError();
        succeeded = 0;
    }
    wpm_progress_finish(&callback.progress, succeeded);
    if (!succeeded) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.error", "repository", "command",
            "Error: transport read failed (HRESULT 0x%08lx, filesystem "
            "error %lu): %s\n",
            (unsigned long)result, (unsigned long)move_error, url);
        DeleteFileA(temporary);
    }
    return succeeded;
}

typedef void *wpm_hinternet;
typedef wpm_hinternet(WINAPI *internet_open_a_fn)(const char *, DWORD,
                                                  const char *, const char *,
                                                  DWORD);
typedef wpm_hinternet(WINAPI *internet_open_url_a_fn)(wpm_hinternet,
                                                      const char *,
                                                      const char *, DWORD,
                                                      DWORD, ULONG_PTR);
typedef BOOL(WINAPI *internet_read_file_fn)(wpm_hinternet, void *, DWORD,
                                            DWORD *);
typedef BOOL(WINAPI *internet_close_handle_fn)(wpm_hinternet);
typedef BOOL(WINAPI *http_query_info_a_fn)(wpm_hinternet, DWORD, void *,
                                           DWORD *, DWORD *);

#define WPM_INTERNET_FLAG_RELOAD 0x80000000UL
#define WPM_INTERNET_FLAG_NO_CACHE_WRITE 0x04000000UL
#define WPM_INTERNET_FLAG_NO_AUTO_REDIRECT 0x00200000UL
#define WPM_INTERNET_FLAG_NO_UI 0x00000200UL
#define WPM_HTTP_QUERY_STATUS_CODE 19UL
#define WPM_HTTP_QUERY_CONTENT_LENGTH 5UL
#define WPM_HTTP_QUERY_LOCATION 33UL
#define WPM_HTTP_QUERY_FLAG_NUMBER 0x20000000UL

static int http_redirect_url(const char *initial, const char *location,
                             char *result, size_t size) {
    size_t origin_length;
    int written;
    if (is_http_repository(location)) {
        return same_web_origin(initial, location) &&
               strcpy_s(result, size, location) == 0;
    }
    if (is_web_repository(location) || location[0] != '/') {
        return 0;
    }
    origin_length = web_origin_length(initial);
    written =
        snprintf(result, size, "%.*s%s", (int)origin_length, initial, location);
    return written > 0 && (size_t)written < size;
}

static int http_content_length(const char *text, unsigned long long *length) {
    int count = 0;
    for (;;) {
        unsigned long long value = 0;
        while (*text == ' ' || *text == '\t') {
            text++;
        }
        if (*text < '0' || *text > '9') {
            return 0;
        }
        do {
            unsigned int digit = (unsigned int)(*text++ - '0');
            if (value > (ULLONG_MAX - digit) / 10) {
                return 0;
            }
            value = value * 10 + digit;
        } while (*text >= '0' && *text <= '9');
        if (count && value != *length) {
            return 0;
        }
        *length = value;
        count++;
        while (*text == ' ' || *text == '\t') {
            text++;
        }
        if (!*text) {
            return 1;
        }
        if (*text++ != ',') {
            return 0;
        }
    }
}
static int http_download(const char *url, const char *destination,
                         const char *label) {
    if (!wpm_require_capability(WPM_CAP_WRITE | WPM_CAP_NETWORK,
                                "http_download")) {
        return 0;
    }
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
    FILE *output = NULL;
    wpm_progress progress;
    unsigned long long received = 0;
    DWORD status = 0;
    DWORD status_size;
    unsigned long long content_length = 0;
    char length_header[PATH_SIZE];
    DWORD failure = ERROR_SUCCESS;
    int length_known = 0;
    DWORD content_length_size;
    DWORD location_size;
    DWORD read;
    int redirect_count;
    int succeeded = 0;
    int progress_started = 0;
    memset(&progress, 0, sizeof(progress));
    if (!library || !is_http_repository(url) ||
        snprintf(temporary, sizeof(temporary), "%s.download", destination) <
            0 ||
        strcpy_s(current, sizeof(current), url) != 0) {
        goto cleanup;
    }
    open_session = (internet_open_a_fn)GetProcAddress(library, "InternetOpenA");
    open_url =
        (internet_open_url_a_fn)GetProcAddress(library, "InternetOpenUrlA");
    read_file =
        (internet_read_file_fn)GetProcAddress(library, "InternetReadFile");
    close_handle = (internet_close_handle_fn)GetProcAddress(
        library, "InternetCloseHandle");
    query_info =
        (http_query_info_a_fn)GetProcAddress(library, "HttpQueryInfoA");
    if (!open_session || !open_url || !read_file || !close_handle ||
        !query_info) {
        goto cleanup;
    }
    session = open_session("WPM/2.0", 0, NULL, NULL, 0);
    if (!session) {
        goto cleanup;
    }
    for (redirect_count = 0; redirect_count <= 5; redirect_count++) {
        request = open_url(
            session, current, NULL, 0,
            WPM_INTERNET_FLAG_RELOAD | WPM_INTERNET_FLAG_NO_CACHE_WRITE |
                WPM_INTERNET_FLAG_NO_AUTO_REDIRECT | WPM_INTERNET_FLAG_NO_UI,
            0);
        if (!request) {
            goto cleanup;
        }
        status_size = sizeof(status);
        if (!query_info(request,
                        WPM_HTTP_QUERY_STATUS_CODE | WPM_HTTP_QUERY_FLAG_NUMBER,
                        &status, &status_size, NULL)) {
            goto cleanup;
        }
        if (status < 300 || status >= 400) {
            break;
        }
        location_size = sizeof(location);
        if (!query_info(request, WPM_HTTP_QUERY_LOCATION, location,
                        &location_size, NULL) ||
            !http_redirect_url(url, location, current, sizeof(current))) {
            wpm_event_printf(
                WPM_EVENT_ERROR, "repository.error", "repository", "command",
                "Error: HTTP redirect changed to a disallowed origin or "
                "scheme.\n");
            goto cleanup;
        }
        close_handle(request);
        request = NULL;
    }
    if (!request || status < 200 || status >= 300) {
        goto cleanup;
    }
    content_length_size = sizeof(length_header);
    length_known = query_info(request, WPM_HTTP_QUERY_CONTENT_LENGTH,
                              length_header, &content_length_size, NULL) != 0;
    if (length_known) {
        if (content_length_size >= sizeof(length_header)) {
            SetLastError(ERROR_INVALID_DATA);
            goto cleanup;
        }
        length_header[content_length_size] = 0;
        if (!http_content_length(length_header, &content_length)) {
            wpm_event_printf(WPM_EVENT_ERROR, "repository.http.length.invalid",
                             "repository", "read",
                             "Error: HTTP Content-Length is invalid.\n");
            SetLastError(ERROR_INVALID_DATA);
            goto cleanup;
        }
        REPO_VERBOSE("HTTP response length: %llu bytes", content_length);
    } else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        SetLastError(ERROR_INVALID_DATA);
        goto cleanup;
    }
    DeleteFileA(temporary);
    output = wpm_fopen(temporary, "wb");
    if (!output) {
        goto cleanup;
    }
    wpm_progress_start(&progress, "Downloading", "Download", "Downloaded",
                       label, content_length);
    progress_started = 1;
    do {
        if (!read_file(request, buffer, sizeof(buffer), &read) ||
            (read && fwrite(buffer, 1, read, output) != read)) {
            goto cleanup;
        }
        if (received > ULLONG_MAX - read) {
            SetLastError(ERROR_INVALID_DATA);
            goto cleanup;
        }
        received += read;
        wpm_progress_set(&progress, received, content_length);
    } while (read);
    if (length_known && received != content_length) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.error", "repository", "command",
            "Error: HTTP response was incomplete (expected %llu bytes, "
            "received %llu).\n",
            content_length, received);
        SetLastError(ERROR_HANDLE_EOF);
        goto cleanup;
    }
    if (fclose(output)) {
        output = NULL;
        goto cleanup;
    }
    output = NULL;
    succeeded = MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING);
cleanup:
    failure = GetLastError();
    if (output) {
        fclose(output);
    }
    if (request && close_handle) {
        close_handle(request);
    }
    if (session && close_handle) {
        close_handle(session);
    }
    if (library) {
        FreeLibrary(library);
    }
    if (progress_started) {
        wpm_progress_finish(&progress, succeeded);
    }
    if (!succeeded) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.error", "repository", "command",
            "Error: insecure HTTP read failed (status %lu, Windows error "
            "%lu): %s\n",
            (unsigned long)status, (unsigned long)failure, current);
        if (temporary[0]) {
            DeleteFileA(temporary);
        }
    }
    return succeeded;
}
static int copy_local(const char *source, const char *destination,
                      const char *label) {
    char temporary[PATH_SIZE];
    HANDLE input = INVALID_HANDLE_VALUE, output = INVALID_HANDLE_VALUE;
    unsigned char buffer[65536];
    DWORD count, failure = ERROR_SUCCESS;
    LARGE_INTEGER size;
    unsigned long long copied = 0;
    int succeeded = 0, progress_started = 0;
    const char *phase = "open source";
    wpm_progress progress;
    int length =
        snprintf(temporary, sizeof(temporary), "%s.download", destination);
    if (length < 0 || (size_t)length >= sizeof(temporary)) {
        return 0;
    }
    DeleteFileA(temporary);
    input = CreateFileA(source, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input == INVALID_HANDLE_VALUE) {
        goto cleanup;
    }
    phase = "read source size";
    if (!GetFileSizeEx(input, &size) || size.QuadPart < 0) {
        goto cleanup;
    }
    phase = "create cache copy";
    output = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) {
        goto cleanup;
    }
    wpm_progress_start(&progress, "Copying", "Copy", "Copied", label,
                       (unsigned long long)size.QuadPart);
    progress_started = 1;
    for (;;) {
        DWORD written;
        phase = "read source";
        if (!ReadFile(input, buffer, sizeof(buffer), &count, NULL)) {
            goto cleanup;
        }
        if (!count) {
            break;
        }
        phase = "write cache copy";
        if (!WriteFile(output, buffer, count, &written, NULL)) {
            goto cleanup;
        }
        if (written != count) {
            SetLastError(ERROR_WRITE_FAULT);
            goto cleanup;
        }
        copied += count;
        wpm_progress_add(&progress, count);
    }
    phase = "verify complete source read";
    if (copied != (unsigned long long)size.QuadPart) {
        SetLastError(ERROR_HANDLE_EOF);
        goto cleanup;
    }
    phase = "flush cache copy";
    if (!FlushFileBuffers(output)) {
        goto cleanup;
    }
    phase = "close cache copy";
    if (!CloseHandle(output)) {
        output = INVALID_HANDLE_VALUE;
        goto cleanup;
    }
    output = INVALID_HANDLE_VALUE;
    phase = "publish cache copy";
    if (!SetFileAttributesA(temporary, FILE_ATTRIBUTE_NORMAL) ||
        !MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING)) {
        goto cleanup;
    }
    succeeded = 1;
cleanup:
    failure = GetLastError();
    if (output != INVALID_HANDLE_VALUE) {
        CloseHandle(output);
    }
    if (input != INVALID_HANDLE_VALUE) {
        CloseHandle(input);
    }
    if (progress_started) {
        wpm_progress_finish(&progress, succeeded);
    }
    if (!succeeded) {
        DeleteFileA(temporary);
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.source.unavailable", "repository",
            "copy",
            "Error: filesystem repository copy failed during %s "
            "(Windows error %lu): %s\n"
            "  Retry: reconnect the original repository media at the "
            "configured path, check read access, then rerun the command. "
            "The previous cache was retained.\n",
            phase, (unsigned long)failure, source);
    }
    return succeeded;
}
static int retrieve(const char *source, const char *destination,
                    const char *label, int allow_insecure_http) {
    if (!wpm_require_capability(WPM_CAP_WRITE, "retrieve")) {
        return 0;
    }
    if (is_https_repository(source)) {
        wpm_https_backend backend = wpm_https_get_backend();
        if (backend == WPM_HTTPS_BUNDLED) {
            return wpm_https_download(source, destination, label);
        }
        if (backend == WPM_HTTPS_URLMON) {
            return urlmon_download(source, destination, label);
        }
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.error", "repository", "command",
            "Error: WPM_HTTPS_BACKEND must be auto, bundled or urlmon; "
            "WPM_TLS_CA_FILE requires auto or bundled.\n");
        return 0;
    }
    if (is_http_repository(source)) {
        if (!allow_insecure_http) {
            return 0;
        }
        wpm_event_printf(
            WPM_EVENT_WARNING, "repository.warning", "repository", "command",
            "Warning: using insecure HTTP transport for %s; content remains "
            "subject "
            "to package signature and trust validation.\n",
            source);
        return http_download(source, destination, label);
    }
    REPO_VERBOSE("copying local source: %s", source);
    return copy_local(source, destination, label);
}
static int index_is_fresh(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    FILETIME now;
    ULARGE_INTEGER a, b;
    GetSystemTimeAsFileTime(&now);
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        return 0;
    }
    a.LowPart = now.dwLowDateTime;
    a.HighPart = now.dwHighDateTime;
    b.LowPart = data.ftLastWriteTime.dwLowDateTime;
    b.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return a.QuadPart >= b.QuadPart &&
           (a.QuadPart - b.QuadPart) <= INDEX_FRESH_MS * 10000ULL;
}
static int index_source(const char *root, char *result, size_t size) {
    return is_web_repository(root)
               ? snprintf(result, size, "%s/index.json", root) > 0
               : join_path(result, size, root, "index.json");
}
static int publish_repository_index(const char *candidate, const char *cached) {
    DWORD error;
    if (!wpm_require_capability(WPM_CAP_WRITE, "publish repository index")) {
        return 0;
    }
    if (MoveFileExA(candidate, cached, MOVEFILE_REPLACE_EXISTING)) {
        return 1;
    }
    error = GetLastError();
    wpm_event_printf(
        WPM_EVENT_WARNING, "repository.cache.publish.failed", "repository",
        "publish",
        "Warning: repository cache publication failed (Windows error %lu): "
        "%s\n  Retry: close programs locking the cache file, check write "
        "access, and rerun 'wpm update'. The previous cache was retained.\n",
        (unsigned long)error, cached);
    return 0;
}
static int index_file_supported(const char *path, int report);
static int refresh(repository *repo, int offline, int required) {
    char cached[PATH_SIZE], url[PATH_SIZE], candidate[PATH_SIZE];
    int length, prepared = 0;
    if (!cache_path(repo->url, cached, sizeof(cached))) {
        return 0;
    }
    REPO_VERBOSE("checking: %s", repo->url);
    REPO_VERBOSE("cache: %s", cached);
    if (offline && is_web_repository(repo->url)) {
        if (GetFileAttributesA(cached) == INVALID_FILE_ATTRIBUTES) {
            wpm_event_printf(
                WPM_EVENT_ERROR, "repository.error", "repository", "command",
                "Error: no cached index for %s while offline.\n", repo->url);
        } else {
            REPO_VERBOSE("using cached index (offline)");
        }
        return GetFileAttributesA(cached) != INVALID_FILE_ATTRIBUTES;
    }
    if (!required && is_web_repository(repo->url) && index_is_fresh(cached)) {
        REPO_VERBOSE("using fresh cached index");
        return 1;
    }
    length = snprintf(candidate, sizeof(candidate), "%s.candidate", cached);
    if (length < 0 || (size_t)length >= sizeof(candidate)) {
        return 0;
    }
    if (index_source(repo->url, url, sizeof(url)) &&
        (prepared = wpm_operation_prepare(url, cached)) &&
        wpm_initialize_data_directories() &&
        ensure_cache_directory("repositories") &&
        retrieve(url, candidate, "repository index",
                 repo->allow_insecure_http) &&
        index_file_supported(candidate, 1) &&
        publish_repository_index(candidate, cached)) {
        wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                         "command", "Updated repository index: %s\n",
                         repo->url);
        return 1;
    }
    if (prepared) {
        DeleteFileA(candidate);
    }
    if (!is_web_repository(repo->url)) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.error", "repository", "command",
            "Error: could not read filesystem repository index: %s\n",
            repo->url);
        return 0;
    }
    if (GetFileAttributesA(cached) != INVALID_FILE_ATTRIBUTES) {
        wpm_event_printf(
            WPM_EVENT_WARNING, "repository.warning", "repository", "command",
            "Warning: could not refresh %s; using cached index.\n", repo->url);
        return 1;
    }
    wpm_event_printf(
        WPM_EVENT_WARNING, "repository.warning", "repository", "command",
        "Warning: could not retrieve repository index: %s\n", repo->url);
    return 0;
}
static int report_available_updates(int offline);
int wpm_repo_update(int offline) {
    wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository", "command",
                     "Updating repositories...\n");
    if (!report_available_updates(offline)) {
        return 0;
    }
    wpm_event_printf(WPM_EVENT_RESULT, "repository.result", "repository",
                     "command", "Result: repositories updated\n");
    return 1;
}

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }
    return p;
}
static int json_hex4(const char **source, unsigned long *value) {
    const char *p = *source;
    unsigned long result = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char c = (unsigned char)*p;
        unsigned long digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return 0;
        }
        result = (result << 4) | digit;
        p++;
    }
    *source = p;
    *value = result;
    return 1;
}
static int json_string(const char **source, char *output, size_t size) {
    const char *p = skip_ws(*source);
    size_t n = 0;
    if (*p != '"') {
        return 0;
    }
    p++;
    while (*p && *p != '"') {
        unsigned long codepoint = (unsigned char)*p++;
        unsigned char bytes[4];
        size_t length = 1;
        if (codepoint < 0x20) {
            return 0;
        }
        if (codepoint == '\\') {
            switch (*p++) {
            case '"':
                codepoint = '"';
                break;
            case '\\':
                codepoint = '\\';
                break;
            case '/':
                codepoint = '/';
                break;
            case 'b':
                codepoint = '\b';
                break;
            case 'f':
                codepoint = '\f';
                break;
            case 'n':
                codepoint = '\n';
                break;
            case 'r':
                codepoint = '\r';
                break;
            case 't':
                codepoint = '\t';
                break;
            case 'u': {
                unsigned long low;
                if (!json_hex4(&p, &codepoint) || (!codepoint && output)) {
                    return 0;
                }
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (*p != '\\' || p[1] != 'u') {
                        return 0;
                    }
                    p += 2;
                    if (!json_hex4(&p, &low) || low < 0xdc00 || low > 0xdfff) {
                        return 0;
                    }
                    codepoint =
                        0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    return 0;
                }
                if (codepoint < 0x80) {
                    bytes[0] = (unsigned char)codepoint;
                } else if (codepoint < 0x800) {
                    length = 2;
                    bytes[0] = (unsigned char)(0xc0 | (codepoint >> 6));
                    bytes[1] = (unsigned char)(0x80 | (codepoint & 0x3f));
                } else if (codepoint < 0x10000) {
                    length = 3;
                    bytes[0] = (unsigned char)(0xe0 | (codepoint >> 12));
                    bytes[1] =
                        (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
                    bytes[2] = (unsigned char)(0x80 | (codepoint & 0x3f));
                } else {
                    length = 4;
                    bytes[0] = (unsigned char)(0xf0 | (codepoint >> 18));
                    bytes[1] =
                        (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f));
                    bytes[2] =
                        (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
                    bytes[3] = (unsigned char)(0x80 | (codepoint & 0x3f));
                }
                if (length >= size || n >= size - length) {
                    return 0;
                }
                if (output) {
                    memcpy(output + n, bytes, length);
                }
                n += length;
                continue;
            }
            default:
                return 0;
            }
        }
        if (n + 1 >= size) {
            return 0;
        }
        if (output) {
            output[n] = (char)codepoint;
        }
        n++;
    }
    if (*p != '"') {
        return 0;
    }
    if (output) {
        output[n] = '\0';
    }
    *source = p + 1;
    return 1;
}
static int json_value_end(const char **source) {
    const char *p = skip_ws(*source);
    int depth = 0, quote = 0;
    while (*p) {
        if (quote) {
            if (*p == '\\' && p[1]) {
                p += 2;
            } else {
                if (*p == '"') {
                    quote = 0;
                }
                p++;
            }
            continue;
        }
        if (*p == '"') {
            quote = 1;
        } else if (*p == '{' || *p == '[') {
            depth++;
        } else if (*p == '}' || *p == ']') {
            if (!depth) {
                break;
            }
            depth--;
        } else if (*p == ',' && depth == 0) {
            break;
        }
        p++;
    }
    *source = p;
    return depth == 0 && !quote;
}
static const char *json_content(const char *text) {
    const unsigned char *bytes = (const unsigned char *)text;
    if (bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf) {
        text += 3;
    }
    return skip_ws(text);
}
/* Iterative grammar validation: depth is bounded only by the 4 MiB input,
 * not by the C call stack. Unknown extension values remain valid JSON. */
static int json_document_valid(const char *text) {
    enum {
        ROOT_VALUE,
        ROOT_END,
        OBJECT_FIRST,
        OBJECT_KEY,
        OBJECT_COLON,
        OBJECT_VALUE,
        OBJECT_END,
        ARRAY_FIRST,
        ARRAY_VALUE,
        ARRAY_END
    };
    const char *p = json_content(text);
    size_t depth = 0, capacity = strlen(p) + 1;
    unsigned char *states = malloc(capacity);
    int valid = 0;
    if (!states) {
        return 0;
    }
    states[0] = ROOT_VALUE;
    for (;;) {
        unsigned char state = states[depth];
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (state == ROOT_END) {
            valid = !*p;
            break;
        }
        if (state == OBJECT_FIRST || state == OBJECT_KEY) {
            if (state == OBJECT_FIRST && *p == '}') {
                p++;
                depth--;
                continue;
            }
            if (!json_string(&p, NULL, (size_t)-1)) {
                break;
            }
            states[depth] = OBJECT_COLON;
            continue;
        }
        if (state == OBJECT_COLON) {
            if (*p != ':') {
                break;
            }
            p++;
            states[depth] = OBJECT_VALUE;
            continue;
        }
        if (state == OBJECT_END || state == ARRAY_END) {
            if (*p == (state == OBJECT_END ? '}' : ']')) {
                p++;
                depth--;
                continue;
            }
            if (*p != ',') {
                break;
            }
            p++;
            states[depth] = state == OBJECT_END ? OBJECT_KEY : ARRAY_VALUE;
            continue;
        }
        if (state == ARRAY_FIRST && *p == ']') {
            p++;
            depth--;
            continue;
        }
        states[depth] = state == ROOT_VALUE     ? ROOT_END
                        : state == OBJECT_VALUE ? OBJECT_END
                                                : ARRAY_END;
        if (*p == '{' || *p == '[') {
            if (depth + 1 >= capacity) {
                break;
            }
            states[++depth] = *p++ == '{' ? OBJECT_FIRST : ARRAY_FIRST;
            continue;
        }
        if (*p == '"') {
            if (!json_string(&p, NULL, (size_t)-1)) {
                break;
            }
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            if (*p == '-') {
                p++;
            }
            if (*p == '0') {
                p++;
            } else {
                if (*p < '1' || *p > '9') {
                    break;
                }
                do {
                    p++;
                } while (*p >= '0' && *p <= '9');
            }
            if (*p == '.') {
                p++;
                if (*p < '0' || *p > '9') {
                    break;
                }
                do {
                    p++;
                } while (*p >= '0' && *p <= '9');
            }
            if (*p == 'e' || *p == 'E') {
                p++;
                if (*p == '+' || *p == '-') {
                    p++;
                }
                if (*p < '0' || *p > '9') {
                    break;
                }
                do {
                    p++;
                } while (*p >= '0' && *p <= '9');
            }
        } else if (!strncmp(p, "true", 4)) {
            p += 4;
        } else if (!strncmp(p, "false", 5)) {
            p += 5;
        } else if (!strncmp(p, "null", 4)) {
            p += 4;
        } else {
            break;
        }
    }
    free(states);
    return valid;
}
/* Find root fields without confusing package metadata with index metadata. */
static const char *index_field(const char *text, const char *wanted) {
    const char *p = json_content(text);
    if (*p != '{') {
        return NULL;
    }
    p++;
    while (*(p = skip_ws(p)) && *p != '}') {
        char key[32];
        if (!json_string(&p, key, sizeof(key))) {
            /* A long or NUL-containing unknown key need not fit our key buffer.
             */
            if (!json_string(&p, NULL, (size_t)-1)) {
                return NULL;
            }
            key[0] = 0;
        }
        if (*(p = skip_ws(p)) != ':') {
            return NULL;
        }
        p = skip_ws(p + 1);
        if (strcmp(key, wanted) == 0) {
            return p;
        }
        json_value_end(&p);
        p = skip_ws(p);
        if (*p != ',') {
            return NULL;
        }
        p++;
    }
    return NULL;
}
static int index_version_supported(const char *text) {
    const char *value = index_field(text, "version");
    char *end;
    double version;
    if (!value || !isdigit((unsigned char)*value)) {
        return 0;
    }
    version = strtod(value, &end);
    end = (char *)skip_ws(end);
    return version == 1.0 && (*end == ',' || *end == '}');
}
/* Validate schema admission before replacing a previous cached catalog. */
static int index_file_supported(const char *path, int report) {
    FILE *input = wpm_fopen(path, "rb");
    char *text = NULL;
    const char *packages;
    long size;
    int supported = 0;
    if (!input) {
        return 0;
    }
    if (fseek(input, 0, SEEK_END) || (size = ftell(input)) < 2 ||
        size > 4 * 1024 * 1024 || fseek(input, 0, SEEK_SET)) {
        goto cleanup;
    }
    text = malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, input) != (size_t)size) {
        goto cleanup;
    }
    text[size] = 0;
    packages = index_field(text, "packages");
    supported = !memchr(text, 0, (size_t)size) && json_document_valid(text) &&
                index_version_supported(text) && packages && *packages == '[';
cleanup:
    free(text);
    fclose(input);
    if (!supported && report) {
        wpm_event_printf(
            WPM_EVENT_WARNING, "repository.index.rejected", "repository",
            "refresh",
            "Warning: refreshed repository index has unsupported schema or "
            "missing top-level packages array or malformed/incomplete "
            "document; "
            "the previous cache was retained.\n");
    }
    return supported;
}
static int append_repository_link(repository *repositories, int *count,
                                  const repository *parent, const char *link);
static int append_repository_link_mode(repository *repositories, int *count,
                                       const repository *parent,
                                       const char *link, int quiet);
static int parse_index(const char *path, repository *repo,
                       package_entry *packages, int *count,
                       repository *repositories, int *repository_count) {
    FILE *f = wpm_fopen(path, "rb");
    long size;
    char *text;
    const char *p;
    int version_ok = 0, before = *count, complete = 1;
    if (!f) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);
    if (size < 2 || size > 4 * 1024 * 1024) {
        fclose(f);
        return 0;
    }
    text = malloc((size_t)size + 1);
    if (!text) {
        fclose(f);
        return 0;
    }
    if (fread(text, 1, (size_t)size, f) != (size_t)size) {
        free(text);
        fclose(f);
        return 0;
    }
    fclose(f);
    text[size] = '\0';
    if (memchr(text, 0, (size_t)size) || !json_document_valid(text)) {
        wpm_event_printf(WPM_EVENT_WARNING, "repository.index.invalid",
                         "repository", "resolve",
                         "Warning: malformed repository JSON: %s\n", repo->url);
        free(text);
        return 0;
    }
    version_ok = index_version_supported(text);
    p = index_field(text, "packages");
    if (!version_ok) {
        wpm_event_printf(
            WPM_EVENT_WARNING, "repository.warning", "repository", "command",
            "Warning: repository index has unsupported schema: %s\n",
            repo->url);
        free(text);
        return 0;
    }
    if (!p || *p != '[') {
        wpm_event_printf(WPM_EVENT_WARNING, "repository.index.invalid",
                         "repository", "resolve",
                         "Warning: repository index needs a top-level "
                         "packages array: %s\n",
                         repo->url);
        free(text);
        return 0;
    }
    p++;
    while (*p && *p != ']') {
        char name[128] = "", version[64] = "", arch[16] = "",
             url[PATH_SIZE] = "";
        p = skip_ws(p);
        if (!*p || *p == ']') {
            break;
        }
        if (*p != '{') {
            json_value_end(&p);
            if (*p != ',') {
                break;
            }
            p++;
            continue;
        }
        p++;
        while (*p && *p != '}') {
            char key[32];
            p = skip_ws(p);
            if (!json_string(&p, key, sizeof(key))) {
                if (!json_string(&p, NULL, (size_t)-1)) {
                    break;
                }
                key[0] = 0;
            }
            p = skip_ws(p);
            if (*p != ':') {
                break;
            }
            p++;
            if (strcmp(key, "name") == 0) {
                json_string(&p, name, sizeof(name));
            } else if (strcmp(key, "version") == 0) {
                json_string(&p, version, sizeof(version));
            } else if (strcmp(key, "arch") == 0) {
                json_string(&p, arch, sizeof(arch));
            } else if (strcmp(key, "url") == 0) {
                json_string(&p, url, sizeof(url));
            } else {
                json_value_end(&p);
            }
            p = skip_ws(p);
            if (*p == ',') {
                p++;
            }
        }
        if (*p == '}') {
            p++;
        }
        if (name[0] && version[0] && arch[0] && url[0]) {
            if (*count < MAX_PACKAGES) {
                package_entry *e = &packages[(*count)++];
                strcpy_s(e->name, sizeof(e->name), name);
                strcpy_s(e->version, sizeof(e->version), version);
                strcpy_s(e->arch, sizeof(e->arch), arch);
                strcpy_s(e->url, sizeof(e->url), url);
                e->priority = repo->priority;
                e->order = repo->order;
            } else {
                complete = 0;
            }
        }
        p = skip_ws(p);
        if (*p == ',') {
            p++;
        }
    }
    REPO_VERBOSE("parsed %d package entries from %s%s", *count - before,
                 repo->url, complete ? "" : " (entry capacity reached)");
    if (!complete) {
        wpm_event_printf(
            WPM_EVENT_WARNING, "repository.warning", "repository", "command",
            "Warning: repository package capacity (%d) was reached while "
            "reading %s; later entries or repositories may be unavailable.\n",
            MAX_PACKAGES, repo->url);
    }
    /* Optional schema-1 links are repository roots, not package URLs. */
    p = index_field(text, "repositories");
    if (p && *p == '[') {
        p++;
        while (*(p = skip_ws(p)) && *p != ']') {
            char link[PATH_SIZE];
            if (!json_string(&p, link, sizeof(link))) {
                break;
            }
            append_repository_link(repositories, repository_count, repo, link);
            p = skip_ws(p);
            if (*p != ',') {
                break;
            }
            p++;
        }
    }
    free(text);
    return 1;
}
static int diagnose_links(const char *path, repository *repositories,
                          int *count, int parent, int *parents, int *issues) {
    FILE *input = wpm_fopen(path, "rb");
    char *text = NULL;
    const char *p;
    long size;
    int result = 0;
    if (!input) {
        return 0;
    }
    if (fseek(input, 0, SEEK_END) || (size = ftell(input)) < 2 ||
        size > 4 * 1024 * 1024 || fseek(input, 0, SEEK_SET)) {
        goto cleanup;
    }
    text = malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, input) != (size_t)size) {
        goto cleanup;
    }
    text[size] = 0;
    if (memchr(text, 0, (size_t)size) || !json_document_valid(text) ||
        !index_version_supported(text)) {
        goto cleanup;
    }
    p = index_field(text, "repositories");
    result = 1;
    if (!p) {
        goto cleanup;
    }
    if (*p != '[') {
        (*issues)++;
        goto cleanup;
    }
    p++;
    while (*(p = skip_ws(p)) && *p != ']') {
        char link[PATH_SIZE];
        int before = *count;
        if (!json_string(&p, link, sizeof(link))) {
            (*issues)++;
            break;
        }
        if (!append_repository_link_mode(repositories, count,
                                         &repositories[parent], link, 1)) {
            (*issues)++;
        } else if (*count > before) {
            parents[before] = parent + 1;
        }
        p = skip_ws(p);
        if (*p != ',') {
            break;
        }
        p++;
    }
cleanup:
    free(text);
    fclose(input);
    return result;
}
static int diagnose_repositories(int follow_links) {
    repository repositories[MAX_REPOSITORIES];
    char path[PATH_SIZE];
    int count, invalid_config = 0, usable = 0, unavailable = 0;
    int invalid = 0, stale = 0, saved_verbose = repository_verbose;
    int configured, issues = 0, parents[MAX_REPOSITORIES] = {0};
    wpm_set_readonly(1);
    repository_verbose =
        0; /* Query output never prints credentialed locators. */
    int loaded = load_repositories(repositories, &count, &invalid_config);
    repository_verbose = saved_verbose;
    if (!loaded) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.health.failed", "repository",
            "diagnose", "Error: repository configuration could not be read.\n");
        return 0;
    }
    configured = count;
    wpm_event_printf(WPM_EVENT_INFO, "repository.health.start", "repository",
                     "diagnose", "Repository health (read-only):\n");
    for (int i = 0; i < count; i++) {
        int web = is_web_repository(repositories[i].url);
        const char *transport = !web ? "filesystem source"
                                : is_http_repository(repositories[i].url)
                                    ? "HTTP cache (insecure)"
                                    : "HTTPS cache";
        const char *status;
        int located =
            web ? cache_path(repositories[i].url, path, sizeof(path))
                : index_source(repositories[i].url, path, sizeof(path));
        if (!located || GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
            unavailable++;
            status = "unavailable";
        } else if (!index_file_supported(path, 0) ||
                   (follow_links && !diagnose_links(path, repositories, &count,
                                                    i, parents, &issues))) {
            invalid++;
            status = "invalid or unreadable index";
        } else {
            usable++;
            if (web && !index_is_fresh(path)) {
                stale++;
                status = "valid, stale";
            } else {
                status = "valid";
            }
        }
        wpm_event_printf(WPM_EVENT_INFO, "repository.health.item", "repository",
                         "diagnose", "  Repository %d: %s; %s\n", i + 1,
                         transport, status);
        if (parents[i]) {
            wpm_event_printf(WPM_EVENT_INFO, "repository.health.parent",
                             "repository", "diagnose",
                             "    Linked from repository %d\n", parents[i]);
        }
    }
    wpm_event_printf(
        WPM_EVENT_INFO, "repository.health.summary", "repository", "diagnose",
        "  Configured roots: %d; usable indexes: %d; unavailable: %d; "
        "invalid: %d; stale: %d; invalid configuration: %d\n",
        configured, usable, unavailable, invalid, stale, invalid_config);
    if (follow_links) {
        wpm_event_printf(
            WPM_EVENT_INFO, "repository.health.links", "repository", "diagnose",
            "  Inspected roots: %d; link issues: %d\n", count, issues);
    }
    if (unavailable || invalid || stale || invalid_config || issues) {
        wpm_event_printf(
            WPM_EVENT_INFO, "repository.health.remediation", "repository",
            "diagnose",
            "  Next action: use 'wpm repo list' to match repository numbers. "
            "Reconnect local media and check index/read access; run "
            "'wpm update' to refresh web caches. Remove or re-add invalid "
            "configuration entries.\n");
        wpm_event_printf(WPM_EVENT_RESULT, "repository.health.attention",
                         "repository", "diagnose",
                         "Result: repository health needs attention\n");
        return 0;
    }
    wpm_event_printf(WPM_EVENT_RESULT, "repository.health.healthy",
                     "repository", "diagnose",
                     "Result: repository health healthy\n");
    return 1;
}
int wpm_repo_diagnose(void) { return diagnose_repositories(0); }
int wpm_repo_diagnose_links(void) { return diagnose_repositories(1); }
static int safe_relative_item(const char *item) {
    const char *segment;
    const char *cursor;
    size_t length;
    if (!item || !*item || item[0] == '/' || item[0] == '\\' ||
        strchr(item, ':') || strstr(item, "://")) {
        return 0;
    }
    segment = item;
    for (cursor = item;; cursor++) {
        if (*cursor != '/' && *cursor != '\\' && *cursor != '\0') {
            continue;
        }
        length = (size_t)(cursor - segment);
        if (!length || (length == 1 && segment[0] == '.') ||
            (length == 2 && segment[0] == '.' && segment[1] == '.')) {
            return 0;
        }
        if (!*cursor) {
            break;
        }
        segment = cursor + 1;
    }
    return 1;
}
static int package_source(const char *root, const char *item, char *result,
                          size_t size) {
    int written;
    if (is_web_repository(root)) {
        if (is_https_repository(item)) {
            return is_https_repository(root) &&
                   strcpy_s(result, size, item) == 0;
        }
        if (is_http_repository(item)) {
            return is_http_repository(root) && same_web_origin(root, item) &&
                   strcpy_s(result, size, item) == 0;
        }
        if (!safe_relative_item(item)) {
            return 0;
        }
        written = snprintf(result, size, "%s/%s", root, item);
        return written > 0 && (size_t)written < size;
    }
    if (!safe_relative_item(item)) {
        return 0;
    }
    return join_path(result, size, root, item);
}

/* Bound the complete graph and deduplicate roots before fetching links. */
static int append_repository_link_mode(repository *repositories, int *count,
                                       const repository *parent,
                                       const char *link, int quiet) {
    char source[PATH_SIZE], normalized[PATH_SIZE];
    if (!package_source(parent->url, link, source, sizeof(source)) ||
        !repository_normalize(source, normalized, sizeof(normalized), 0)) {
        if (!quiet) {
            wpm_event_printf(
                WPM_EVENT_WARNING, "repository.link.invalid", "repository",
                "resolve", "Warning: ignoring unsafe repository link in %s.\n",
                parent->url);
        }
        return 0;
    }
    for (int i = 0; i < *count; i++) {
        if (_stricmp(repositories[i].url, normalized) == 0) {
            return 1;
        }
    }
    if (*count >= MAX_REPOSITORIES) {
        if (!quiet) {
            wpm_event_printf(
                WPM_EVENT_WARNING, "repository.link.capacity", "repository",
                "resolve", "Warning: repository link capacity (%d) reached.\n",
                MAX_REPOSITORIES);
        }
        return 0;
    }
    repository *child = &repositories[*count];
    *child = *parent;
    child->order = *count;
    strcpy_s(child->url, sizeof(child->url), normalized);
    (*count)++;
    return 1;
}
static int append_repository_link(repository *repositories, int *count,
                                  const repository *parent, const char *link) {
    return append_repository_link_mode(repositories, count, parent, link, 0);
}
typedef struct {
    unsigned long long major, minor, patch;
    const char *prerelease;
    size_t prerelease_length;
} semver;
typedef struct {
    char name[128];
    char version[64];
    char arch[16];
} installed_entry;

static int parse_number(const char **p, unsigned long long *value) {
    const char *start = *p;
    unsigned long long result = 0;
    if (!isdigit((unsigned char)*start)) {
        return 0;
    }
    if (*start == '0' && isdigit((unsigned char)start[1])) {
        return 0;
    }
    while (isdigit((unsigned char)**p)) {
        unsigned digit = (unsigned)(**p - '0');
        if (result > (ULLONG_MAX - digit) / 10) {
            return 0;
        }
        result = result * 10 + digit;
        (*p)++;
    }
    *value = result;
    return *p > start;
}
static int valid_identifiers(const char *p, const char *end, int prerelease) {
    while (p < end) {
        const char *start = p;
        int numeric = 1;
        while (p < end && *p != '.') {
            if (!isalnum((unsigned char)*p) && *p != '-') {
                return 0;
            }
            if (!isdigit((unsigned char)*p)) {
                numeric = 0;
            }
            p++;
        }
        if (p == start ||
            (prerelease && numeric && p - start > 1 && *start == '0')) {
            return 0;
        }
        if (p < end) {
            p++;
        }
    }
    return 1;
}
static int semver_parse(const char *text, semver *result) {
    const char *p = text, *pre = NULL, *plus = NULL, *end;
    if (!text || !parse_number(&p, &result->major) || *p++ != '.' ||
        !parse_number(&p, &result->minor) || *p++ != '.' ||
        !parse_number(&p, &result->patch)) {
        return 0;
    }
    if (*p == '-') {
        pre = ++p;
        while (*p && *p != '+') {
            p++;
        }
    }
    if (*p == '+') {
        plus = ++p;
        while (*p) {
            p++;
        }
    }
    if (*p) {
        return 0;
    }
    end = plus ? plus - 1 : p;
    if (pre && (pre == end || !valid_identifiers(pre, end, 1))) {
        return 0;
    }
    if (plus && (plus == p || !valid_identifiers(plus, p, 0))) {
        return 0;
    }
    result->prerelease = pre;
    result->prerelease_length = pre ? (size_t)(end - pre) : 0;
    return 1;
}
static int identifier_compare(const char *a, size_t an, const char *b,
                              size_t bn) {
    int ad = 1, bd = 1;
    size_t i, n;
    for (i = 0; i < an; i++) {
        if (!isdigit((unsigned char)a[i])) {
            ad = 0;
        }
    }
    for (i = 0; i < bn; i++) {
        if (!isdigit((unsigned char)b[i])) {
            bd = 0;
        }
    }
    if (ad && bd) {
        while (an > 1 && *a == '0') {
            a++;
            an--;
        }
        while (bn > 1 && *b == '0') {
            b++;
            bn--;
        }
        if (an != bn) {
            return an > bn ? 1 : -1;
        }
    } else if (ad != bd) {
        return ad ? -1 : 1;
    }
    n = an < bn ? an : bn;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return (unsigned char)a[i] > (unsigned char)b[i] ? 1 : -1;
        }
    }
    return an == bn ? 0 : (an > bn ? 1 : -1);
}
static int semver_compare_parsed(const semver *a, const semver *b) {
    const char *ap, *bp, *ae, *be;
    if (a->major != b->major) {
        return a->major > b->major ? 1 : -1;
    }
    if (a->minor != b->minor) {
        return a->minor > b->minor ? 1 : -1;
    }
    if (a->patch != b->patch) {
        return a->patch > b->patch ? 1 : -1;
    }
    if (!a->prerelease && !b->prerelease) {
        return 0;
    }
    if (!a->prerelease) {
        return 1;
    }
    if (!b->prerelease) {
        return -1;
    }
    ap = a->prerelease;
    bp = b->prerelease;
    ae = ap + a->prerelease_length;
    be = bp + b->prerelease_length;
    while (ap < ae && bp < be) {
        const char *ax = memchr(ap, '.', (size_t)(ae - ap)),
                   *bx = memchr(bp, '.', (size_t)(be - bp));
        size_t an = ax ? (size_t)(ax - ap) : (size_t)(ae - ap),
               bn = bx ? (size_t)(bx - bp) : (size_t)(be - bp);
        int c = identifier_compare(ap, an, bp, bn);
        if (c) {
            return c;
        }
        ap += an + (ax != NULL);
        bp += bn + (bx != NULL);
    }
    return ap == ae ? (bp == be ? 0 : -1) : 1;
}
static int semver_compare(const char *a, const char *b, int *valid) {
    semver av, bv;
    if (!semver_parse(a, &av) || !semver_parse(b, &bv)) {
        if (valid) {
            *valid = 0;
        }
        return 0;
    }
    if (valid) {
        *valid = 1;
    }
    return semver_compare_parsed(&av, &bv);
}
static int is_prerelease(const char *value) {
    semver parsed;
    return semver_parse(value, &parsed) && parsed.prerelease != NULL;
}

int wpm_repo_version_is_valid(const char *version) {
    semver parsed;
    return version && semver_parse(version, &parsed);
}

static int prerelease_path(char *path, size_t size) {
    char root[PATH_SIZE];
    return wpm_get_data_root(root, sizeof(root)) &&
           join_path(path, size, root, "config\\prerelease.txt");
}
static int prerelease_effective(const char *package, int *overridden) {
    char path[PATH_SIZE], line[512];
    FILE *f;
    int global = 0, value = 0, found = 0;
    if (overridden) {
        *overridden = 0;
    }
    if (!prerelease_path(path, sizeof(path))) {
        return 0;
    }
    f = wpm_fopen(path, "r");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        char *tab;
        line[strcspn(line, "\r\n")] = 0;
        if (_strnicmp(line, "global=", 7) == 0) {
            global = _stricmp(line + 7, "true") == 0;
        } else if ((tab = strchr(line, '\t')) != NULL) {
            *tab = 0;
            if (package && _stricmp(line, package) == 0) {
                value = _stricmp(tab + 1, "true") == 0;
                found = 1;
            }
        }
    }
    fclose(f);
    if (found) {
        if (overridden) {
            *overridden = 1;
        }
        return value;
    }
    return global;
}
static int rewrite_prerelease(const char *package, int enabled, int remove) {
    if (!wpm_require_capability(WPM_CAP_WRITE, "rewrite_prerelease")) {
        return 0;
    }
    char path[PATH_SIZE], tmp[PATH_SIZE], line[512];
    FILE *in, *out;
    int found = 0;
    if (!prerelease_path(path, sizeof(path)) ||
        snprintf(tmp, sizeof(tmp), "%s.tmp", path) < 0) {
        return 0;
    }
    in = wpm_fopen(path, "r");
    out = wpm_fopen(tmp, "w");
    if (!out) {
        if (in) {
            fclose(in);
        }
        return 0;
    }
    if (in) {
        while (fgets(line, sizeof(line), in)) {
            char copy[512], *tab;
            strcpy_s(copy, sizeof(copy), line);
            line[strcspn(line, "\r\n")] = 0;
            if (!package && _strnicmp(line, "global=", 7) == 0) {
                found = 1;
                if (!remove) {
                    fprintf(out, "global=%s\n", enabled ? "true" : "false");
                }
            } else if (package && (tab = strchr(line, '\t')) != NULL) {
                *tab = 0;
                if (_stricmp(line, package) == 0) {
                    found = 1;
                    if (!remove) {
                        fprintf(out, "%s\t%s\n", package,
                                enabled ? "true" : "false");
                    }
                } else {
                    fputs(copy, out);
                }
            } else {
                fputs(copy, out);
            }
        }
        fclose(in);
    }
    if (!remove && !found) {
        if (package) {
            fprintf(out, "%s\t%s\n", package, enabled ? "true" : "false");
        } else {
            fprintf(out, "global=%s\n", enabled ? "true" : "false");
        }
    }
    if (fclose(out) || !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp);
        return 0;
    }
    return !remove || found;
}
int wpm_config_prerelease_set(const char *package_name, int enabled) {
    const char *scope = package_name ? package_name : "global";
    if (!rewrite_prerelease(package_name, enabled, 0)) {
        wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                         "command",
                         "Error: could not save prerelease configuration.\n");
        return 0;
    }
    wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository", "command",
                     "Prerelease %s=%s.\n", scope, enabled ? "true" : "false");
    wpm_event_printf(WPM_EVENT_RESULT, "repository.result", "repository",
                     "command", "Result: prerelease configured %s %s\n", scope,
                     enabled ? "true" : "false");
    return 1;
}
int wpm_config_prerelease_get(const char *package_name) {
    int overridden = 0, value = prerelease_effective(package_name, &overridden);
    wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository", "command",
                     "prerelease=%s (%s)\n", value ? "true" : "false",
                     package_name
                         ? (overridden ? "package override" : "global setting")
                         : "global setting");
    return 1;
}
int wpm_config_prerelease_unset(const char *package_name) {
    if (!package_name || !rewrite_prerelease(package_name, 0, 1)) {
        wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                         "command",
                         "Error: package prerelease override was not found.\n");
        return 0;
    }
    wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository", "command",
                     "Prerelease override removed: %s\n", package_name);
    wpm_event_printf(WPM_EVENT_RESULT, "repository.result", "repository",
                     "command", "Result: prerelease override removed %s\n",
                     package_name);
    return 1;
}

static int load_entries(repository *repositories, int *repository_count,
                        package_entry *entries, int *entry_count, int offline,
                        int force_refresh) {
    char cached[PATH_SIZE];
    int usable = 0;
    *entry_count = 0;
    if (!load_repositories(repositories, repository_count, NULL)) {
        return 0;
    }
    for (int i = 0; i < *repository_count; i++) {
        if (wpm_is_readonly() || (wpm_operation_active() &&
                                  (!is_web_repository(repositories[i].url) ||
                                   !wpm_operation_collecting()))) {
            int available =
                is_web_repository(repositories[i].url)
                    ? cache_path(repositories[i].url, cached, sizeof(cached))
                    : index_source(repositories[i].url, cached, sizeof(cached));
            if (available &&
                GetFileAttributesA(cached) != INVALID_FILE_ATTRIBUTES &&
                parse_index(cached, &repositories[i], entries, entry_count,
                            repositories, repository_count)) {
                usable = 1;
            } else {
                wpm_event_printf(
                    WPM_EVENT_WARNING, "plan.source.unavailable", "plan",
                    "resolve",
                    "Warning: planning needs an existing readable index for "
                    "%s; "
                    "use 'wpm update' before planning a web repository.\n",
                    repositories[i].url);
            }
            continue;
        }
        if (refresh(&repositories[i], offline, force_refresh) &&
            cache_path(repositories[i].url, cached, sizeof(cached))) {
            if (parse_index(cached, &repositories[i], entries, entry_count,
                            repositories, repository_count)) {
                usable = 1;
            }
        }
    }
    REPO_VERBOSE("resolver loaded %d entries from %d repositories",
                 *entry_count, *repository_count);
    return usable;
}
static int load_installed(installed_entry *result, int *count) {
    char root[PATH_SIZE], store[PATH_SIZE], search[PATH_SIZE], path[PATH_SIZE];
    WIN32_FIND_DATAA item;
    HANDLE find;
    *count = 0;
    if (!wpm_get_data_root(root, sizeof(root)) ||
        !join_path(store, sizeof(store), root, "packages") ||
        !join_path(search, sizeof(search), store, "*.zip")) {
        return 0;
    }
    find = FindFirstFileA(search, &item);
    if (find == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    do {
        wpm_package_info info;
        semver parsed;
        if (*count >= MAX_INSTALLED ||
            !join_path(path, sizeof(path), store, item.cFileName)) {
            continue;
        }
        if (!wpm_archive_inspect(path, &info)) {
            wpm_event_printf(
                WPM_EVENT_WARNING, "repository.warning", "repository",
                "command",
                "Warning: unreadable installed package record: %s\n  Path: "
                "%s\n",
                item.cFileName, path);
            continue;
        }
        if (!semver_parse(info.version, &parsed)) {
            wpm_event_printf(
                WPM_EVENT_WARNING, "repository.warning", "repository",
                "command",
                "Warning: installed package record has non-SemVer version "
                "'%s': %s\n  Path: %s\n  This legacy record is ignored for "
                "update selection; review the archive before deciding "
                "whether it is obsolete.\n",
                info.version, item.cFileName, path);
            continue;
        }
        installed_entry *e = &result[(*count)++];
        strcpy_s(e->name, sizeof(e->name), info.name);
        strcpy_s(e->version, sizeof(e->version), info.version);
        strcpy_s(e->arch, sizeof(e->arch), info.arch);
    } while (FindNextFileA(find, &item));
    FindClose(find);
    return 1;
}
static int better_candidate(package_entry *candidate, package_entry *selected) {
    int valid, c;
    if (!selected) {
        return 1;
    }
    c = semver_compare(candidate->version, selected->version, &valid);
    if (!valid) {
        return 0;
    }
    return c > 0 || (c == 0 && (candidate->priority > selected->priority ||
                                (candidate->priority == selected->priority &&
                                 candidate->order < selected->order)));
}
static package_entry *select_candidate(package_entry *entries, int count,
                                       const char *name, const char *arch,
                                       const char *exact_version,
                                       int install_mode) {
    package_entry *selected = NULL;
    const char *available_arch = NULL;
    int allow_pre = prerelease_effective(name, NULL), named = 0,
        version_matched = 0, arch_matched = 0, prerelease_excluded = 0,
        architecture_excluded = 0;
    for (int i = 0; i < count; i++) {
        semver parsed;
        if (_stricmp(entries[i].name, name) != 0) {
            continue;
        }
        named++;
        if (!semver_parse(entries[i].version, &parsed)) {
            wpm_event_printf(WPM_EVENT_WARNING, "repository.warning",
                             "repository", "command",
                             "Warning: ignoring invalid SemVer %s for %s.\n",
                             entries[i].version, name);
            continue;
        }
        if (exact_version && strcmp(entries[i].version, exact_version) != 0) {
            continue;
        }
        if (is_prerelease(entries[i].version) && !allow_pre) {
            if ((install_mode && !arch &&
                 (_stricmp(entries[i].arch, WPM_TARGET_ARCH) == 0 ||
                  _stricmp(entries[i].arch, "any") == 0)) ||
                ((!install_mode || arch) &&
                 _stricmp(entries[i].arch, arch) == 0)) {
                prerelease_excluded++;
            }
            continue;
        }
        version_matched++;
        if (install_mode && !arch) {
            if (_stricmp(entries[i].arch, WPM_TARGET_ARCH) != 0 &&
                _stricmp(entries[i].arch, "any") != 0) {
                architecture_excluded++;
                if (!available_arch) {
                    available_arch = entries[i].arch;
                }
                continue;
            }
            if (selected && _stricmp(selected->arch, WPM_TARGET_ARCH) == 0 &&
                _stricmp(entries[i].arch, "any") == 0) {
                continue;
            }
            if (selected && _stricmp(selected->arch, "any") == 0 &&
                _stricmp(entries[i].arch, WPM_TARGET_ARCH) == 0) {
                selected = &entries[i];
                arch_matched++;
                continue;
            }
        } else if (_stricmp(entries[i].arch, arch) != 0) {
            architecture_excluded++;
            if (!available_arch) {
                available_arch = entries[i].arch;
            }
            continue;
        }
        arch_matched++;
        if (better_candidate(&entries[i], selected)) {
            selected = &entries[i];
        }
    }
    REPO_VERBOSE("resolution for '%s': name matches=%d, version/prerelease "
                 "matches=%d, architecture matches=%d, prereleases "
                 "excluded=%d, architectures excluded=%d, selected=%s",
                 name, named, version_matched, arch_matched,
                 prerelease_excluded, architecture_excluded,
                 selected ? selected->version : "none");
    if (install_mode && !selected && prerelease_excluded) {
        wpm_event_printf(
            WPM_EVENT_WARNING, "repository.warning", "repository", "command",
            "Warning: matching prerelease packages were excluded because "
            "prereleases are disabled.\n  Enable them for this package "
            "with: wpm config set prerelease true --package %s\n",
            name);
    } else if (install_mode && !selected && architecture_excluded) {
        wpm_event_printf(
            WPM_EVENT_WARNING, "repository.warning", "repository", "command",
            "Warning: matching packages were found, but none support "
            "architecture %s.\n  An available architecture is: %s\n  Retry "
            "with: wpm install %s --arch %s\n",
            arch ? arch : WPM_TARGET_ARCH, available_arch, name,
            available_arch);
    }
    return selected;
}
static int obtain(repository *repositories, package_entry *selected,
                  int offline, char *path, size_t size) {
    repository *repo = &repositories[selected->order];
    char root[PATH_SIZE], source[PATH_SIZE], legacy[PATH_SIZE], label[256];
    if (strpbrk(selected->name, "\\/:*") ||
        strpbrk(selected->version, "\\/:*") ||
        strpbrk(selected->arch, "\\/:*") ||
        !package_source(repo->url, selected->url, source, sizeof(source)) ||
        !wpm_get_data_root(root, sizeof(root)) ||
        snprintf(path, size, "%s\\cache\\packages\\%s-%s-%s.zip", root,
                 selected->name, selected->arch, selected->version) < 0 ||
        snprintf(legacy, sizeof(legacy), "%s\\cache\\packages\\%s-%s-%s.zip",
                 root, selected->name, selected->version, selected->arch) < 0 ||
        snprintf(label, sizeof(label), "package %s %s %s", selected->name,
                 selected->version, selected->arch) < 0) {
        wpm_event_printf(WPM_EVENT_ERROR, "repository.package.invalid",
                         "repository", "resolve",
                         "Error: invalid package URL in repository index.\n");
        return 0;
    }
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesA(legacy) != INVALID_FILE_ATTRIBUTES) {
        strcpy_s(path, size, legacy);
    }
    if (wpm_operation_active() && !is_web_repository(repo->url)) {
        if (strcpy_s(path, size, source) == 0 &&
            GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
        wpm_event_printf(
            WPM_EVENT_ERROR, "plan.source.unavailable", "plan", "resolve",
            "Error: selected local package is unavailable: %s\n"
            "  Retry: reconnect the original repository media at the "
            "configured path, check read access, and recompute the plan.\n",
            source);
        return 0;
    }
    if (wpm_is_readonly() &&
        GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "plan.package.unavailable", "plan", "resolve",
            "Error: dry-run requires an already cached web package: %s; "
            "prepare the cache with an approved install before planning.\n",
            selected->name);
        return 0;
    }
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        if (offline && is_web_repository(repo->url)) {
            wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                             "command",
                             "Error: package is not cached while offline: %s\n",
                             selected->name);
            return 0;
        }
        if (!wpm_operation_prepare(source, path) ||
            !wpm_initialize_data_directories() ||
            !ensure_cache_directory("packages") ||
            !retrieve(source, path, label, repo->allow_insecure_http)) {
            wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                             "command",
                             "Error: could not retrieve package: %s\n", source);
            return 0;
        }
    }
    return 1;
}

static int candidate_matches(const char *archive,
                             const package_entry *selected);

int wpm_repo_install(const char *package_name, const char *arch,
                     const char *version, int offline, int allow_unsigned) {
    repository repositories[MAX_REPOSITORIES];
    package_entry entries[MAX_PACKAGES], *selected;
    int rc, ec, result;
    char package[PATH_SIZE], selected_source_path[PATH_SIZE];
    semver parsed;
    if (version && !semver_parse(version, &parsed)) {
        wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                         "command",
                         "Error: --version requires valid SemVer.\n");
        return 0;
    }
    if (arch && _stricmp(arch, "any") && _stricmp(arch, "x86") &&
        _stricmp(arch, "x64") && _stricmp(arch, "arm64")) {
        wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                         "command", "Error: unsupported architecture: %s\n",
                         arch);
        return 0;
    }
    if (!load_entries(repositories, &rc, entries, &ec, offline, 0)) {
        return 0;
    }
    selected = select_candidate(entries, ec, package_name, arch, version, 1);
    if (!selected) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "repository.error", "repository", "command",
            "Error: package was not found with the requested selectors: %s\n",
            package_name);
        return 0;
    }
    if (!obtain(repositories, selected, offline, package, sizeof(package))) {
        return 0;
    }
    if (!candidate_matches(package, selected)) {
        return 0;
    }
    wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository", "command",
                     "Installing %s %s %s from %s\n", selected->name,
                     selected->arch, selected->version,
                     repositories[selected->order].url);
    if (!package_source(repositories[selected->order].url, selected->url,
                        selected_source_path, sizeof(selected_source_path))) {
        return 0;
    }
    wpm_operation_set_source(selected_source_path);
    wpm_archive_set_repository_url(repositories[selected->order].url);
    result = wpm_archive_install(package, allow_unsigned);
    wpm_operation_set_source(NULL);
    wpm_archive_set_repository_url(NULL);
    return result;
}

static int candidate_matches(const char *archive,
                             const package_entry *selected) {
    wpm_package_info identity;
    if (!wpm_archive_inspect(archive, &identity) ||
        _stricmp(identity.name, selected->name) ||
        _stricmp(identity.arch, selected->arch) ||
        strcmp(identity.version, selected->version)) {
        wpm_event_printf(WPM_EVENT_ERROR, "plan.identity.changed", "plan",
                         "resolve",
                         "Error: downloaded package metadata does not match "
                         "the selected repository entry.\n");
        return 0;
    }
    return 1;
}

static int requested(const char *name, const char **names, int count) {
    for (int i = 0; i < count; i++) {
        if (_stricmp(name, names[i]) == 0) {
            return 1;
        }
    }
    return 0;
}
static int installed_is_latest(installed_entry *installed, int count,
                               int index) {
    int valid;
    for (int j = 0; j < count; j++) {
        if (index != j &&
            _stricmp(installed[index].name, installed[j].name) == 0 &&
            _stricmp(installed[index].arch, installed[j].arch) == 0 &&
            semver_compare(installed[j].version, installed[index].version,
                           &valid) > 0) {
            return 0;
        }
    }
    return 1;
}
static int conflicting_any(installed_entry *installed, int count, int index) {
    if (_stricmp(installed[index].arch, "any") != 0) {
        return 0;
    }
    for (int j = 0; j < count; j++) {
        if (index != j &&
            _stricmp(installed[index].name, installed[j].name) == 0 &&
            _stricmp(installed[j].arch, "any") != 0) {
            return 1;
        }
    }
    return 0;
}
static package_entry *newer_candidate(package_entry *entries, int entry_count,
                                      installed_entry *item,
                                      const char *exact_version) {
    package_entry *selected = select_candidate(entries, entry_count, item->name,
                                               item->arch, exact_version, 0);
    int valid;
    if (!selected ||
        semver_compare(selected->version, item->version, &valid) <= 0 ||
        !valid) {
        return NULL;
    }
    return selected;
}
static int print_upgrade_plan(installed_entry *installed, int installed_count,
                              package_entry *entries, int entry_count,
                              const char *arch) {
    int count = 0;
    for (int i = 0; i < installed_count; i++) {
        package_entry *selected;
        if ((arch && _stricmp(arch, installed[i].arch) != 0) ||
            !installed_is_latest(installed, installed_count, i) ||
            conflicting_any(installed, installed_count, i)) {
            continue;
        }
        selected = newer_candidate(entries, entry_count, &installed[i], NULL);
        if (!selected) {
            continue;
        }
        if (!count) {
            wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                             "command", "Planned upgrades:\n");
        }
        wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                         "command", "  %s %s: %s -> %s\n", installed[i].name,
                         installed[i].arch, installed[i].version,
                         selected->version);
        count++;
    }
    return count;
}
static int confirm_upgrade_plan(int assume_yes) {
    char answer[16];
    HANDLE input;
    DWORD read = 0;
    size_t end;
    if (assume_yes) {
        return 1;
    }
    wpm_event_printf(WPM_EVENT_PROMPT, "repository.prompt", "repository",
                     "command", "Proceed with these upgrades? [y/N]: ");
    fflush(stdout);
    input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == NULL || input == INVALID_HANDLE_VALUE ||
        !ReadFile(input, answer, sizeof(answer) - 1, &read, NULL) ||
        read == 0) {
        return 0;
    }
    answer[read] = '\0';
    end = strcspn(answer, "\r\n");
    answer[end] = '\0';
    return _stricmp(answer, "y") == 0 || _stricmp(answer, "yes") == 0;
}

static int count_upgrade_candidates(installed_entry *installed,
                                    int installed_count, package_entry *entries,
                                    int entry_count, const char **package_names,
                                    int package_count, int all,
                                    const char *arch, const char *version) {
    int count = 0;
    for (int i = 0; i < installed_count; i++) {
        package_entry *selected;
        int valid;
        if (!all &&
            !requested(installed[i].name, package_names, package_count)) {
            continue;
        }
        if (arch && _stricmp(arch, installed[i].arch) != 0) {
            continue;
        }
        if (!installed_is_latest(installed, installed_count, i) ||
            conflicting_any(installed, installed_count, i)) {
            continue;
        }
        selected = select_candidate(entries, entry_count, installed[i].name,
                                    installed[i].arch, version, 0);
        if (selected &&
            semver_compare(selected->version, installed[i].version, &valid) >
                0 &&
            valid) {
            count++;
        }
    }
    return count;
}

static int report_available_updates(int offline) {
    repository repositories[MAX_REPOSITORIES];
    package_entry entries[MAX_PACKAGES];
    installed_entry installed[MAX_INSTALLED];
    int rc, ec, ic, count;
    if (!load_installed(installed, &ic) ||
        !load_entries(repositories, &rc, entries, &ec, offline, 1)) {
        return 0;
    }
    count = print_upgrade_plan(installed, ic, entries, ec, NULL);
    if (!count) {
        wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                         "command", "All installed packages are current.\n");
    } else {
        wpm_event_printf(
            WPM_EVENT_INFO, "repository.info", "repository", "command",
            "%d package %s can be upgraded. Run 'wpm upgrade --all' to "
            "review and apply.\n",
            count, count == 1 ? "identity" : "identities");
    }
    return 1;
}

int wpm_repo_upgrade(const char **package_names, int package_count, int all,
                     const char *arch, const char *version, int offline,
                     int allow_unsigned, int assume_yes) {
    repository repositories[MAX_REPOSITORIES];
    package_entry entries[MAX_PACKAGES];
    installed_entry installed[MAX_INSTALLED];
    int rc, ec, ic, failures = 0, processed = 0, progress_current = 0,
                    progress_total;
    semver parsed;
    if (version && !semver_parse(version, &parsed)) {
        wpm_event_printf(WPM_EVENT_ERROR, "repository.error", "repository",
                         "command",
                         "Error: --version requires valid SemVer.\n");
        return 0;
    }
    if (!load_installed(installed, &ic) ||
        !load_entries(repositories, &rc, entries, &ec, offline, 0)) {
        return 0;
    }
    progress_total =
        count_upgrade_candidates(installed, ic, entries, ec, package_names,
                                 package_count, all, arch, version);
    if (all && !wpm_operation_collecting()) {
        int planned = print_upgrade_plan(installed, ic, entries, ec, arch);
        if (planned && !confirm_upgrade_plan(assume_yes)) {
            wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                             "command",
                             "Upgrade cancelled; no packages were changed.\n");
            return 1;
        }
        if (planned) {
            wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                             "command", "\n");
        }
    }
    for (int i = 0; i < ic; i++) {
        package_entry *selected;
        int valid, comparison, operation_ok;
        char cached[PATH_SIZE], selected_source_path[PATH_SIZE];
        int current = 1, conflict = 0, is_self;
        if (!all &&
            !requested(installed[i].name, package_names, package_count)) {
            continue;
        }
        if (arch && _stricmp(arch, installed[i].arch) != 0) {
            continue;
        }
        current = installed_is_latest(installed, ic, i);
        conflict = conflicting_any(installed, ic, i);
        if (!current) {
            continue;
        }
        if (conflict) {
            wpm_event_printf(
                WPM_EVENT_WARNING, "repository.warning", "repository",
                "command",
                "Warning: ignoring conflicting any installation for %s.\n",
                installed[i].name);
            if (arch && _stricmp(arch, "any") == 0) {
                failures++;
            }
            continue;
        }
        processed++;
        selected = select_candidate(entries, ec, installed[i].name,
                                    installed[i].arch, version, 0);
        if (!selected) {
            wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                             "command", "%s %s %s is current.\n",
                             installed[i].name, installed[i].arch,
                             installed[i].version);
            wpm_event_printf(WPM_EVENT_RESULT, "repository.result",
                             "repository", "command", "Result: %s %s current\n",
                             installed[i].name, installed[i].arch);
            continue;
        }
        comparison =
            semver_compare(selected->version, installed[i].version, &valid);
        if (!valid || comparison <= 0) {
            if (version) {
                wpm_event_printf(
                    WPM_EVENT_ERROR, "repository.error", "repository",
                    "command",
                    "Error: requested upgrade version must be newer than "
                    "installed %s.\n",
                    installed[i].version);
                wpm_event_printf(WPM_EVENT_RESULT, "repository.result",
                                 "repository", "command",
                                 "Result: %s %s failed\n", installed[i].name,
                                 installed[i].arch);
                failures++;
            } else {
                wpm_event_printf(WPM_EVENT_INFO, "repository.info",
                                 "repository", "command",
                                 "%s %s %s is current.\n", installed[i].name,
                                 installed[i].arch, installed[i].version);
                wpm_event_printf(WPM_EVENT_RESULT, "repository.result",
                                 "repository", "command",
                                 "Result: %s %s current\n", installed[i].name,
                                 installed[i].arch);
            }
            continue;
        }
        is_self = _stricmp(selected->name, "wpm") == 0;
        wpm_archive_set_progress(++progress_current, progress_total);
        if (!obtain(repositories, selected, offline, cached, sizeof(cached))) {
            wpm_event_printf(WPM_EVENT_RESULT, "repository.result",
                             "repository", "command", "Result: %s %s failed\n",
                             installed[i].name, installed[i].arch);
            failures++;
            continue;
        }
        if (!candidate_matches(cached, selected)) {
            failures++;
            continue;
        }
        if (!package_source(repositories[selected->order].url, selected->url,
                            selected_source_path,
                            sizeof(selected_source_path))) {
            failures++;
            continue;
        }
        wpm_operation_set_source(selected_source_path);
        wpm_archive_set_repository_url(repositories[selected->order].url);
        operation_ok =
            is_self ? wpm_archive_schedule_self_upgrade(
                          cached, allow_unsigned, selected->version,
                          selected->arch, installed[i].version)
                    : wpm_archive_upgrade(cached, allow_unsigned,
                                          selected->name, selected->version,
                                          selected->arch, installed[i].version);
        wpm_archive_set_repository_url(NULL);
        wpm_operation_set_source(NULL);
        if (!operation_ok) {
            wpm_event_printf(WPM_EVENT_RESULT, "repository.result",
                             "repository", "command", "Result: %s %s failed\n",
                             installed[i].name, installed[i].arch);
            failures++;
            continue;
        }
        if (!wpm_operation_collecting()) {
            wpm_event_printf(WPM_EVENT_RESULT, "repository.result",
                             "repository", "command", "Result: %s %s %s\n",
                             installed[i].name, installed[i].arch,
                             is_self ? "scheduled" : "upgraded");
        }
    }
    for (int n = 0; n < package_count; n++) {
        int found = 0;
        for (int i = 0; i < ic; i++) {
            if (_stricmp(package_names[n], installed[i].name) == 0 &&
                (!arch || _stricmp(arch, installed[i].arch) == 0)) {
                found = 1;
            }
        }
        if (!found) {
            wpm_event_printf(
                WPM_EVENT_ERROR, "repository.error", "repository", "command",
                "Error: requested package identity is not installed: %s%s%s\n",
                package_names[n], arch ? " " : "", arch ? arch : "");
            failures++;
        }
    }
    if (!processed && !failures) {
        wpm_event_printf(WPM_EVENT_INFO, "repository.info", "repository",
                         "command",
                         "No installed packages matched the request.\n");
        return 0;
    }
    return failures == 0;
}
