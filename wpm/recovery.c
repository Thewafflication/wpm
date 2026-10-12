/** @file recovery.c @brief Bounded read-only upgrade failure audit query. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include "archive.h"
#include "capability.h"
#include "logging.h"
#include "recovery.h"

#define RECOVERY_PATH 4096
#define RECOVERY_LIMIT 128

typedef struct {
    char file[MAX_PATH], name[128], arch[16], old_version[64], new_version[64];
    char archive[MAX_PATH], phase[64];
    DWORD exit_code;
} failure_record;

static int recovery_join(char *path, size_t size, const char *root,
                         const char *item) {
    int written = snprintf(path, size, "%s%s%s", root,
                           root[strlen(root) - 1] == '\\' ? "" : "\\", item);
    return written > 0 && (size_t)written < size;
}

static int recovery_record(const char *path, failure_record *record) {
    HANDLE input = INVALID_HANDLE_VALUE;
    char text[16385], *line;
    DWORD size, high = 0, read;
    unsigned fields = 0;
    int success = 0;
    input = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input == INVALID_HANDLE_VALUE) {
        return 0;
    }
    size = GetFileSize(input, &high);
    if (high || !size || size >= sizeof(text) ||
        !ReadFile(input, text, size, &read, NULL) || read != size ||
        memchr(text, 0, size)) {
        goto done;
    }
    text[size] = 0;
    line = text;
    while (*line) {
        char *next = strpbrk(line, "\r\n");
        char *value, *destination = NULL;
        size_t capacity = 0;
        unsigned field = 0;
        if (next) {
            *next++ = 0;
            while (*next == '\r' || *next == '\n') {
                next++;
            }
        }
        value = strchr(line, '=');
        if (!value) {
            goto done;
        }
        *value++ = 0;
        if (!strcmp(line, "name")) {
            destination = record->name;
            capacity = sizeof(record->name);
            field = 1;
        } else if (!strcmp(line, "arch")) {
            destination = record->arch;
            capacity = sizeof(record->arch);
            field = 2;
        } else if (!strcmp(line, "old-version")) {
            destination = record->old_version;
            capacity = sizeof(record->old_version);
            field = 4;
        } else if (!strcmp(line, "new-version")) {
            destination = record->new_version;
            capacity = sizeof(record->new_version);
            field = 8;
        } else if (!strcmp(line, "archive")) {
            destination = record->archive;
            capacity = sizeof(record->archive);
            field = 16;
        } else if (!strcmp(line, "phase")) {
            destination = record->phase;
            capacity = sizeof(record->phase);
            field = 32;
        } else if (!strcmp(line, "status")) {
            if (strcmp(value, "failed")) {
                goto done;
            }
            field = 64;
        } else if (!strcmp(line, "verification")) {
            if (strcmp(value, "failed")) {
                goto done;
            }
            field = 128;
        } else if (!strcmp(line, "exit-code")) {
            uint64_t number = 0;
            const char *p = value;
            if (!*p) {
                goto done;
            }
            while (*p) {
                unsigned digit = (unsigned char)*p++ - '0';
                if (digit > 9 || number > (0xffffffffULL - digit) / 10) {
                    goto done;
                }
                number = number * 10 + digit;
            }
            record->exit_code = (DWORD)number;
            field = 256;
        }
        if ((fields & field) ||
            (destination && (!*value || strlen(value) >= capacity ||
                             strcpy_s(destination, capacity, value)))) {
            goto done;
        }
        fields |= field;
        line = next ? next : line + strlen(line) + 1 + strlen(value);
    }
    success =
        fields == 511 &&
        (!_stricmp(record->arch, "any") || !_stricmp(record->arch, "x86") ||
         !_stricmp(record->arch, "x64") || !_stricmp(record->arch, "arm64")) &&
        !strpbrk(record->archive, "\\/:") && strcmp(record->archive, ".") &&
        strcmp(record->archive, "..");
done:
    CloseHandle(input);
    return success;
}

static int recovery_order(const void *left, const void *right) {
    const failure_record *a = left, *b = right;
    return _stricmp(b->file, a->file);
}

int wpm_recovery_list(void) { return wpm_recovery_list_limit(RECOVERY_LIMIT); }

int wpm_recovery_list_limit(unsigned limit) {
    char root[RECOVERY_PATH], directory[RECOVERY_PATH], path[RECOVERY_PATH];
    failure_record *records;
    WIN32_FIND_DATAA found;
    HANDLE search = INVALID_HANDLE_VALUE;
    DWORD attributes, failure;
    int count = 0, scanned = 0, invalid = 0, limited = 0, success = 0;
    wpm_set_readonly(1);
    if (!limit || limit > 4096) {
        return 0;
    }
    records = calloc(limit, sizeof(*records));
    if (!records || !wpm_get_data_root(root, sizeof(root)) || !*root ||
        !recovery_join(directory, sizeof(directory), root, "audit")) {
        goto done;
    }
    attributes = GetFileAttributesA(directory);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        failure = GetLastError();
        if (failure == ERROR_FILE_NOT_FOUND ||
            failure == ERROR_PATH_NOT_FOUND) {
            success = 1;
            goto report;
        }
        goto done;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
        !recovery_join(path, sizeof(path), directory, "*.upgrade-failed.txt")) {
        goto done;
    }
    search = FindFirstFileA(path, &found);
    if (search == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            success = 1;
            goto report;
        }
        goto done;
    }
    do {
        if ((unsigned)scanned == limit) {
            limited = 1;
            break;
        }
        scanned++;
        if ((found.dwFileAttributes &
             (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
            !recovery_join(path, sizeof(path), directory, found.cFileName) ||
            !recovery_record(path, &records[count])) {
            invalid++;
            memset(&records[count], 0, sizeof(records[count]));
            continue;
        }
        strcpy_s(records[count].file, sizeof(records[count].file),
                 found.cFileName);
        count++;
    } while (FindNextFileA(search, &found));
    if (!limited && GetLastError() != ERROR_NO_MORE_FILES) {
        invalid++;
    }
    success = !invalid && !limited;
report:
    qsort(records, (size_t)count, sizeof(*records), recovery_order);
    wpm_event_printf(WPM_EVENT_INFO, "recovery.audit.start", "recovery", "list",
                     "Recorded upgrade failures (read-only):\n");
    for (int i = 0; i < count; i++) {
        wpm_event_printf(
            WPM_EVENT_INFO, "recovery.audit.item", "recovery", "list",
            "  Record: %s\n  %s %s: %s -> %s\n"
            "  Phase: %s; exit code: %lu\n"
            "  Archive reference: %s\n",
            records[i].file, records[i].name, records[i].arch,
            records[i].old_version, records[i].new_version, records[i].phase,
            (unsigned long)records[i].exit_code, records[i].archive);
    }
    wpm_event_printf(WPM_EVENT_RESULT, "recovery.audit.summary", "recovery",
                     "list",
                     "Result: %d recorded upgrade failures; "
                     "invalid/unreadable records: %d; limited: %s\n",
                     count, invalid, limited ? "yes" : "no");
    if (count || invalid || limited) {
        wpm_event_printf(
            WPM_EVENT_INFO, "recovery.audit.next", "recovery", "list",
            "Next action: inspect the named audit and retained source/archive "
            "with the package maintainer before retrying. Revalidate "
            "integrity, "
            "trust, and current state; a failed script may already have "
            "changed "
            "software. This query does not retry or clean up operations.\n");
    }
done:
    if (search != INVALID_HANDLE_VALUE) {
        FindClose(search);
    }
    free(records);
    if (!success && !invalid && !limited) {
        wpm_event_printf(WPM_EVENT_ERROR, "recovery.audit.failed", "recovery",
                         "list",
                         "Error: failure audits could not be read. "
                         "Check data-root/audit access and layout.\n");
    }
    return fflush(stdout) == 0 && !ferror(stdout) && success;
}
