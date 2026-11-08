/** @file copy.c @brief Plan, confirm, release file users and copy payloads. */
#include "copy.h"
#include "capability.h"
#include "file_processes.h"
#include "logging.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct {
    WCHAR source[MAX_PATH], destination[MAX_PATH];
    int directory;
} copy_entry;
typedef struct {
    copy_entry *entries;
    size_t count, capacity, files;
} copy_plan;

static int failed(const char *operation, DWORD error) {
    wpm_event_printf(WPM_EVENT_ERROR, "copy.failed", "copy", operation,
                     "Copy failed: %s (Windows error %lu).\n", operation,
                     (unsigned long)error);
    return 1;
}

static int join(WCHAR *out, const WCHAR *parent, const WCHAR *name) {
    size_t length = wcslen(parent);
    if (length + wcslen(name) + 2 > MAX_PATH) {
        return failed("path exceeds MAX_PATH", ERROR_FILENAME_EXCED_RANGE);
    }
    wcscpy(out, parent);
    if (length && out[length - 1] != L'\\') {
        wcscat(out, L"\\");
    }
    wcscat(out, name);
    return 0;
}

static int ordinary_name(const WCHAR *name, size_t length) {
    WCHAR base[MAX_PATH];
    size_t stem = 0;
    if (!length || name[length - 1] == L'.' || name[length - 1] == L' ') {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        if (name[i] < 32 || wcschr(L"<>:\"|?*", name[i])) {
            return 0;
        }
    }
    while (stem < length && name[stem] != L'.') {
        base[stem] = name[stem];
        stem++;
    }
    base[stem] = 0;
    if (!lstrcmpiW(base, L"CON") || !lstrcmpiW(base, L"PRN") ||
        !lstrcmpiW(base, L"AUX") || !lstrcmpiW(base, L"NUL") ||
        !lstrcmpiW(base, L"CONIN$") || !lstrcmpiW(base, L"CONOUT$")) {
        return 0;
    }
    if (stem == 4 && (wcschr(L"123456789\x00b9\x00b2\x00b3", base[3]))) {
        base[3] = 0;
        if (!lstrcmpiW(base, L"COM") || !lstrcmpiW(base, L"LPT")) {
            return 0;
        }
    }
    return 1;
}

/* Inspect every existing component. Do not traverse junctions or symlinks. */
static int inspect(const WCHAR *path) {
    WCHAR part[MAX_PATH];
    size_t length = wcslen(path);
    wcscpy(part, path);
    for (size_t start = 0, i = 0; i <= length; i++) {
        if (!path[i] || path[i] == L'\\') {
            if (i > start && !(start == 0 && i == 2 && path[1] == L':') &&
                !ordinary_name(path + start, i - start)) {
                return failed("unsupported path component", ERROR_INVALID_NAME);
            }
            start = i + 1;
        }
    }
    for (size_t i = 0; i <= length; i++) {
        WCHAR saved;
        DWORD attributes, error;
        if (i < 3 || (part[i] != L'\\' && part[i])) {
            continue;
        }
        /* A UNC server alone is not a filesystem object. */
        if (part[0] == L'\\' && part[1] == L'\\' &&
            !wmemchr(part + 2, L'\\', i - 2)) {
            continue;
        }
        saved = part[i];
        part[i] = 0;
        attributes = GetFileAttributesW(part);
        error = GetLastError();
        part[i] = saved;
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            if (error == ERROR_FILE_NOT_FOUND ||
                error == ERROR_PATH_NOT_FOUND) {
                continue;
            }
            return failed("inspect path", error);
        }
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            return failed("reparse points are not supported",
                          ERROR_INVALID_NAME);
        }
        if (i < length && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            return failed("parent is not a directory", ERROR_DIRECTORY);
        }
    }
    return 0;
}

static int resolve(const char *input, WCHAR *out) {
    WCHAR wide[MAX_PATH], prefix[MAX_PATH], expanded[MAX_PATH];
    DWORD length;
    size_t cut;
    if (!input[0] ||
        !MultiByteToWideChar(CP_ACP, 0, input, -1, wide, MAX_PATH)) {
        return failed("convert path", ERROR_INVALID_NAME);
    }
    for (size_t i = 0; wide[i]; i++) {
        if (wide[i] == L'/') {
            wide[i] = L'\\';
        }
        if (wide[i] == L'*' || wide[i] == L'?' || (wide[i] == L':' && i != 1)) {
            return failed("expected a filesystem path", ERROR_INVALID_NAME);
        }
    }
    if (!wcsncmp(wide, L"\\\\.\\", 4)) {
        return failed("device paths are not supported", ERROR_INVALID_NAME);
    }
    for (size_t start = 0, i = 0;; i++) {
        if (!wide[i] || wide[i] == L'\\') {
            size_t count = i - start;
            int dot =
                (count == 1 && wide[start] == L'.') ||
                (count == 2 && wide[start] == L'.' && wide[start + 1] == L'.');
            if (count && !dot && !(start == 0 && i == 2 && wide[1] == L':') &&
                !ordinary_name(wide + start, count)) {
                return failed("unsupported path component", ERROR_INVALID_NAME);
            }
            start = i + 1;
        }
        if (!wide[i]) {
            break;
        }
    }
    length = GetFullPathNameW(wide, MAX_PATH, out, NULL);
    if (!length || length >= MAX_PATH) {
        return failed("resolve path", ERROR_FILENAME_EXCED_RANGE);
    }
    while (length > 3 && out[length - 1] == L'\\') {
        out[--length] = 0;
    }
    /* Windows strips trailing dots/spaces. Refuse ambiguous component names. */
    for (size_t i = 0; i < length; i++) {
        if ((out[i] == L'.' || out[i] == L' ') &&
            (!out[i + 1] || out[i + 1] == L'\\')) {
            return failed("ambiguous path component", ERROR_INVALID_NAME);
        }
    }
    if (inspect(out)) {
        return 1;
    }
    /* Expand 8.3 aliases in the longest existing prefix before overlap checks.
     */
    wcscpy(prefix, out);
    cut = length;
    for (;;) {
        DWORD attributes = GetFileAttributesW(prefix);
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            DWORD size = GetLongPathNameW(prefix, expanded, MAX_PATH);
            if (!size || size >= MAX_PATH ||
                size + wcslen(out + cut) >= MAX_PATH) {
                return failed("expand path", ERROR_FILENAME_EXCED_RANGE);
            }
            wcscat(expanded, out + cut);
            wcscpy(out, expanded);
            return 0;
        }
        while (cut && prefix[cut - 1] != L'\\') {
            cut--;
        }
        if (cut < 3) {
            return failed("locate existing path root", ERROR_PATH_NOT_FOUND);
        }
        if (cut == 3 && prefix[1] == L':') {
            prefix[cut] = 0;
        } else {
            prefix[--cut] = 0;
        }
    }
}

static int contains(const WCHAR *parent, const WCHAR *path) {
    size_t length = wcslen(parent);
    WCHAR prefix[MAX_PATH];
    if (wcslen(path) < length) {
        return 0;
    }
    wmemcpy(prefix, path, length);
    prefix[length] = 0;
    return !lstrcmpiW(parent, prefix) &&
           (!path[length] || parent[length - 1] == L'\\' ||
            path[length] == L'\\');
}

static int same_file(const WCHAR *source, const WCHAR *destination) {
    BY_HANDLE_FILE_INFORMATION a, b;
    HANDLE left = CreateFileW(
        source, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    HANDLE right = CreateFileW(
        destination, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    int same = left != INVALID_HANDLE_VALUE && right != INVALID_HANDLE_VALUE &&
               GetFileInformationByHandle(left, &a) &&
               GetFileInformationByHandle(right, &b) &&
               a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
               a.nFileIndexHigh == b.nFileIndexHigh &&
               a.nFileIndexLow == b.nFileIndexLow;
    if (left != INVALID_HANDLE_VALUE) {
        CloseHandle(left);
    }
    if (right != INVALID_HANDLE_VALUE) {
        CloseHandle(right);
    }
    return same;
}

static int collect(copy_plan *plan, const WCHAR *source,
                   const WCHAR *destination, unsigned depth) {
    DWORD attributes = GetFileAttributesW(source), target, error;
    int directory;
    if (depth > 128 || attributes == INVALID_FILE_ATTRIBUTES) {
        return failed("inspect source",
                      depth > 128 ? ERROR_DIRECTORY : GetLastError());
    }
    if (inspect(source) || inspect(destination)) {
        return 1;
    }
    directory = !!(attributes & FILE_ATTRIBUTE_DIRECTORY);
    target = GetFileAttributesW(destination);
    error = GetLastError();
    if (target == INVALID_FILE_ATTRIBUTES && error != ERROR_FILE_NOT_FOUND &&
        error != ERROR_PATH_NOT_FOUND) {
        return failed("inspect destination", error);
    }
    if ((target != INVALID_FILE_ATTRIBUTES &&
         directory != !!(target & FILE_ATTRIBUTE_DIRECTORY)) ||
        same_file(source, destination)) {
        return failed("source/destination conflict", ERROR_INVALID_PARAMETER);
    }
    if (plan->count == plan->capacity) {
        size_t capacity = plan->capacity ? plan->capacity * 2 : 32;
        copy_entry *entries;
        if (capacity > 65536) {
            return failed("copy plan exceeds 65536 entries", ERROR_OUTOFMEMORY);
        }
        entries = realloc(plan->entries, capacity * sizeof(*entries));
        if (!entries) {
            return failed("allocate copy plan", ERROR_OUTOFMEMORY);
        }
        plan->entries = entries;
        plan->capacity = capacity;
    }
    wcscpy(plan->entries[plan->count].source, source);
    wcscpy(plan->entries[plan->count].destination, destination);
    plan->entries[plan->count++].directory = directory;
    if (!directory) {
        plan->files++;
        return 0;
    }
    {
        WCHAR pattern[MAX_PATH];
        WIN32_FIND_DATAW found;
        HANDLE search;
        int result = 0;
        if (join(pattern, source, L"*")) {
            return 1;
        }
        search = FindFirstFileW(pattern, &found);
        if (search == INVALID_HANDLE_VALUE) {
            error = GetLastError();
            return error == ERROR_FILE_NOT_FOUND
                       ? 0
                       : failed("enumerate source", error);
        }
        do {
            WCHAR child[MAX_PATH], output[MAX_PATH];
            if (!wcscmp(found.cFileName, L".") ||
                !wcscmp(found.cFileName, L"..")) {
                continue;
            }
            if (join(child, source, found.cFileName) ||
                join(output, destination, found.cFileName) ||
                collect(plan, child, output, depth + 1)) {
                result = 1;
                break;
            }
        } while (FindNextFileW(search, &found));
        error = GetLastError();
        FindClose(search);
        if (!result && error != ERROR_NO_MORE_FILES) {
            result = failed("enumerate source", error);
        }
        return result;
    }
}

static int make_directory(const WCHAR *path) {
    WCHAR parent[MAX_PATH], *separator;
    DWORD attributes = GetFileAttributesW(path);
    if (inspect(path)) {
        return 1;
    }
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return attributes & FILE_ATTRIBUTE_DIRECTORY
                   ? 0
                   : failed("directory conflict", ERROR_DIRECTORY);
    }
    wcscpy(parent, path);
    separator = wcsrchr(parent, L'\\');
    if (!separator || separator == parent) {
        return failed("directory root unavailable", ERROR_PATH_NOT_FOUND);
    }
    if (separator == parent + 2 && parent[1] == L':') {
        separator[1] = 0;
    } else {
        *separator = 0;
    }
    if (make_directory(parent)) {
        return 1;
    }
    if (!CreateDirectoryW(path, NULL)) {
        return failed("create destination directory", GetLastError());
    }
    return 0;
}

/* Reuse the exact-file utility as one batch, preserving deduplication and all
 * legacy, service, ancestor and process-identity safeguards. */
static int file_users(copy_plan *plan, int kill, int legacy) {
    char **arguments = calloc(plan->files + 3, sizeof(char *));
    int count = 0, result = 1;
    if (!arguments) {
        return failed("allocate file-user arguments", ERROR_OUTOFMEMORY);
    }
    for (size_t i = 0; i < plan->count; i++) {
        BOOL replaced = FALSE;
        if (plan->entries[i].directory) {
            continue;
        }
        arguments[count] = calloc(MAX_PATH * 2, 1);
        if (!arguments[count] ||
            !WideCharToMultiByte(
                CP_ACP, WC_NO_BEST_FIT_CHARS, plan->entries[i].destination, -1,
                arguments[count], MAX_PATH * 2, NULL, &replaced) ||
            replaced) {
            failed("convert destination", ERROR_NO_UNICODE_TRANSLATION);
            goto cleanup;
        }
        count++;
    }
    if (!count) {
        result = 0;
        goto cleanup;
    }
    /* Options come after paths, whose resolved names cannot begin with '-'. */
    if (kill) {
        arguments[count++] = "--kill";
    }
    if (legacy) {
        arguments[count++] = "--legacy";
    }
    result = wpm_file_processes(count, arguments);
cleanup:
    for (size_t i = 0; i < plan->files; i++) {
        free(arguments[i]);
    }
    free(arguments);
    return result;
}

int wpm_copy(int argc, char **argv) {
    const char *operands[2];
    int number = 0, recursive = 0, yes = 0, legacy = 0, literal = 0;
    int dry_run = wpm_is_readonly(), result = 1;
    WCHAR source[MAX_PATH], destination[MAX_PATH], output[MAX_PATH];
    DWORD attributes;
    copy_plan plan = {0};
    for (int i = 0; i < argc; i++) {
        if (!literal && !strcmp(argv[i], "--")) {
            literal = 1;
        } else if (!literal && (!strcmp(argv[i], "--recursive") ||
                                !strcmp(argv[i], "-r"))) {
            recursive = 1;
        } else if (!literal &&
                   (!strcmp(argv[i], "--yes") || !strcmp(argv[i], "-y"))) {
            yes = 1;
        } else if (!literal && !strcmp(argv[i], "--legacy")) {
            legacy = 1;
        } else if (!literal && !strcmp(argv[i], "--dry-run")) {
            dry_run = 1;
        } else if (!literal && !strcmp(argv[i], "--kill")) {
            /* Accepted for explicit installer intent; confirmation still
             * applies. */
        } else if (!literal &&
                   (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))) {
            wpm_event_printf(
                WPM_EVENT_INFO, "copy.help", "copy", "help",
                "Usage: wpm copy <source> <destination> [--recursive|-r] "
                "[--yes|-y] [--dry-run] [--legacy] [--kill]\n"
                "Copy files or directory contents, stopping destination file "
                "users.\n"
                "Prompts before stopping applications and overwriting. "
                "--yes skips confirmation; --dry-run makes no changes.\n");
            return 0;
        } else if ((!literal && argv[i][0] == '-') || number == 2) {
            return failed("invalid copy arguments", ERROR_INVALID_PARAMETER);
        } else {
            operands[number++] = argv[i];
        }
    }
    if (number != 2) {
        return failed("source and destination required",
                      ERROR_INVALID_PARAMETER);
    }
    if (resolve(operands[0], source) || resolve(operands[1], destination)) {
        return 1;
    }
    attributes = GetFileAttributesW(source);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return failed("source not found", GetLastError());
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (!recursive) {
            return failed("directory requires --recursive or -r",
                          ERROR_INVALID_PARAMETER);
        }
    } else if ((GetFileAttributesW(destination) != INVALID_FILE_ATTRIBUTES) &&
               (GetFileAttributesW(destination) & FILE_ATTRIBUTE_DIRECTORY)) {
        if (join(output, destination, wcsrchr(source, L'\\') + 1)) {
            return 1;
        }
        wcscpy(destination, output);
    }
    if (contains(source, destination) || contains(destination, source)) {
        return failed("source and destination overlap",
                      ERROR_INVALID_PARAMETER);
    }
    if (collect(&plan, source, destination, 0)) {
        goto cleanup;
    }
    wpm_event_printf(WPM_EVENT_INFO, "copy.plan", "copy", "plan",
                     "Copy %lu file(s): %s -> %s\n", (unsigned long)plan.files,
                     operands[0], operands[1]);
    for (size_t i = 0; i < plan.count; i++) {
        wpm_event_printf(WPM_EVENT_INFO, "copy.entry", "copy", "plan",
                         "%s: %ls -> %ls\n",
                         plan.entries[i].directory ? "Directory" : "File",
                         plan.entries[i].source, plan.entries[i].destination);
    }
    if (file_users(&plan, 0, legacy)) {
        goto cleanup;
    }
    if (dry_run) {
        result = 0;
        goto cleanup;
    }
    if (!yes) {
        char answer[32];
        wpm_event_printf(WPM_EVENT_PROMPT, "copy.confirm", "copy", "confirm",
                         "Stop applications using these destination files and "
                         "copy/overwrite? "
                         "Unsaved work may be lost. [y/N]: ");
        fflush(stdout);
        if (!fgets(answer, sizeof(answer), stdin) ||
            (lstrcmpiA(answer, "y\n") && lstrcmpiA(answer, "yes\n"))) {
            failed("cancelled; no files copied", ERROR_CANCELLED);
            goto cleanup;
        }
    }
    if (!wpm_require_capability(WPM_CAP_WRITE | WPM_CAP_SCRIPT,
                                "copy files and stop file users")) {
        goto cleanup;
    }
    /* Revalidate after the prompt, before terminating any application. */
    {
        copy_plan checked = {0};
        if (collect(&checked, source, destination, 0)) {
            free(checked.entries);
            goto cleanup;
        }
        int changed = checked.count != plan.count;
        for (size_t i = 0; !changed && i < plan.count; i++) {
            changed =
                checked.entries[i].directory != plan.entries[i].directory ||
                wcscmp(checked.entries[i].source, plan.entries[i].source) ||
                wcscmp(checked.entries[i].destination,
                       plan.entries[i].destination);
        }
        if (changed) {
            free(checked.entries);
            failed("copy plan changed; run the command again", ERROR_RETRY);
            goto cleanup;
        }
        free(checked.entries);
    }
    if (file_users(&plan, 1, legacy)) {
        goto cleanup;
    }
    for (size_t i = 0; i < plan.count; i++) {
        copy_entry *entry = &plan.entries[i];
        if (inspect(entry->source) || inspect(entry->destination)) {
            goto cleanup;
        }
        if (entry->directory) {
            if (make_directory(entry->destination)) {
                goto cleanup;
            }
        } else {
            WCHAR parent[MAX_PATH], *separator;
            wcscpy(parent, entry->destination);
            separator = wcsrchr(parent, L'\\');
            if (separator == parent + 2) {
                separator[1] = 0;
            } else {
                *separator = 0;
            }
            if (make_directory(parent) ||
                same_file(entry->source, entry->destination)) {
                failed("destination parent or file identity conflict",
                       ERROR_INVALID_PARAMETER);
                goto cleanup;
            }
            if (!CopyFileW(entry->source, entry->destination, FALSE)) {
                failed(
                    "copy destination file; earlier files may have been copied",
                    GetLastError());
                goto cleanup;
            }
        }
    }
    wpm_event_printf(WPM_EVENT_RESULT, "copy.complete", "copy", "copy",
                     "Copied %lu file(s).\n", (unsigned long)plan.files);
    result = 0;
cleanup:
    free(plan.entries);
    return result;
}
