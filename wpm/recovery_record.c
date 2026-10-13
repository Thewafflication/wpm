/** @file recovery_record.c @brief Read-only admission of recovery identities.
 * Recovery documents are unauthenticated diagnostic inputs. Their paths never
 * grant read or execution authority: current archive validation uses an
 * explicitly supplied path only. Query bounds are 4096 documents, 64 KiB each;
 * path guards and exclusive-read handles reject reparse/hard-linked records.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <windows.h>
#include "archive.h"
#include "capability.h"
#include "hashing.h"
#include "json_input.h"
#include "json_output.h"
#include "logging.h"
#include "path_guard.h"
#include "recovery.h"
#include "repository.h"

#define RECORD_PATH 4096
#define RECORD_BYTES 65536
#define RECORD_READ_BUDGET (64U * 1024U * 1024U)

typedef struct {
    char id[33], operation[16], created[32], updated[32], plan[65];
    char status[16], phase[32], name[128], arch[16];
    char old_version[64], new_version[64], source[RECORD_PATH];
    char input[RECORD_PATH], fingerprint[65], signer[65];
    unsigned long script_exit;
    int external, exit_known;
    const char *input_status;
    char retained_relative[256], retained_state[16];
    char retained_path[RECORD_PATH];
    const char *retained_status;
    int retained_known, inventory_known;
} recovery_identity;

int wpm_recovery_id_valid(const char *id) {
    if (!id || strlen(id) != 32) {
        return 0;
    }
    for (const char *p = id; *p; p++) {
        if (!(*p >= '0' && *p <= '9') && !(*p >= 'a' && *p <= 'f')) {
            return 0;
        }
    }
    return 1;
}

static int record_hex(const char *text) {
    if (strlen(text) != 64) {
        return 0;
    }
    for (const char *p = text; *p; p++) {
        if (!(*p >= '0' && *p <= '9') && !(*p >= 'a' && *p <= 'f')) {
            return 0;
        }
    }
    return 1;
}

static int record_delimiter(const char *p) {
    p = wpm_json_space(p);
    return *p == ',' || *p == '}';
}

static int record_string(const char *text, const char *key, char *output,
                         size_t size, int nullable) {
    const char *p = wpm_json_unique_field(text, key);
    if (!p) {
        return 0;
    }
    if (nullable && !strncmp(p, "null", 4) && record_delimiter(p + 4)) {
        output[0] = 0;
        return 1;
    }
    return wpm_json_read_string(&p, output, size) && *output &&
           record_delimiter(p);
}

static int record_bool(const char *text, const char *key, int *value) {
    const char *p = wpm_json_unique_field(text, key);
    if (p && !strncmp(p, "true", 4) && record_delimiter(p + 4)) {
        *value = 1;
        return 1;
    }
    if (p && !strncmp(p, "false", 5) && record_delimiter(p + 5)) {
        *value = 0;
        return 1;
    }
    return 0;
}

static int record_time(const char *text) {
    static const int positions[] = {4, 7, 10, 13, 16, 19, 23};
    static const char separators[] = "--T::.Z";
    int year, month, day, days;
    if (strlen(text) != 24) {
        return 0;
    }
    for (int i = 0; i < 24; i++) {
        int separator = 0;
        for (int j = 0; j < 7; j++) {
            if (positions[j] == i) {
                if (text[i] != separators[j]) {
                    return 0;
                }
                separator = 1;
            }
        }
        if (!separator && (text[i] < '0' || text[i] > '9')) {
            return 0;
        }
    }
    year = atoi(text);
    month = atoi(text + 5);
    day = atoi(text + 8);
    if (!year || month < 1 || month > 12 || day < 1 || atoi(text + 11) > 23 ||
        atoi(text + 14) > 59 || atoi(text + 17) > 59) {
        return 0;
    }
    days = month == 2 ? 28 + (!(year % 4) && (year % 100 || !(year % 400)))
           : month == 4 || month == 6 || month == 9 || month == 11 ? 30
                                                                   : 31;
    return day <= days;
}

static int record_artifacts(const char *artifacts, recovery_identity *record) {
    const char *p = wpm_json_space(artifacts + 1);
    unsigned count = 0;
    record->inventory_known = 1;
    record->retained_status = "not-recorded";
    while (*p != ']') {
        char role[32], digest[65], path[RECORD_PATH];
        if (*p != '{' || ++count > 16 ||
            !record_string(p, "role", role, sizeof(role), 0)) {
            return 0;
        }
        if (!strcmp(role, "recovery-input")) {
            if (record->retained_known ||
                !record_string(p, "path", path, sizeof(path), 0) ||
                !record_string(p, "relativePath", record->retained_relative,
                               sizeof(record->retained_relative), 0) ||
                !record_string(p, "blake2b256", digest, sizeof(digest), 0) ||
                strcmp(digest, record->fingerprint) ||
                !record_string(p, "state", record->retained_state,
                               sizeof(record->retained_state), 0) ||
                (strcmp(record->retained_state, "planned") &&
                 strcmp(record->retained_state, "incomplete") &&
                 strcmp(record->retained_state, "retained"))) {
                return 0;
            }
            record->retained_known = 1;
            record->retained_status = "not-checked";
        } else {
            record->inventory_known = 0;
        }
        if (!wpm_json_value_end(&p)) {
            return 0;
        }
        p = wpm_json_space(p);
        if (*p != ',') {
            break;
        }
        p = wpm_json_space(p + 1);
    }
    return *p == ']' && record_delimiter(p + 1);
}

static int record_parse(const char *text, const char *id,
                        recovery_identity *record) {
    const char *package, *input, *exit;
    char schema[32], validation[48], failed_phase[32];
    const char *actions, *artifacts, *history;
    int retry;
    if (!wpm_json_document_valid(text) || !wpm_json_utf8_valid(text)) {
        return 0;
    }
    package = wpm_json_unique_field(text, "package");
    input = wpm_json_unique_field(text, "input");
    exit = wpm_json_unique_field(text, "scriptExitCode");
    actions = wpm_json_unique_field(text, "safeActions");
    artifacts = wpm_json_unique_field(text, "controlledArtifacts");
    history = wpm_json_unique_field(text, "completedPhases");
    if (!package || *package != '{' || !input || *input != '{' || !exit ||
        !record_string(text, "schema", schema, sizeof(schema), 0) ||
        strcmp(schema, "wpm.recovery.v1") ||
        !record_string(text, "operationId", record->id, sizeof(record->id),
                       0) ||
        strcmp(id, record->id) ||
        !record_string(text, "operation", record->operation,
                       sizeof(record->operation), 0) ||
        !record_string(text, "created", record->created,
                       sizeof(record->created), 0) ||
        !record_string(text, "updated", record->updated,
                       sizeof(record->updated), 0) ||
        !record_string(text, "planDigest", record->plan, sizeof(record->plan),
                       0) ||
        !record_string(text, "status", record->status, sizeof(record->status),
                       0) ||
        !record_string(text, "phase", record->phase, sizeof(record->phase),
                       0) ||
        !record_string(package, "name", record->name, sizeof(record->name),
                       0) ||
        !record_string(package, "arch", record->arch, sizeof(record->arch),
                       0) ||
        !record_string(package, "oldVersion", record->old_version,
                       sizeof(record->old_version), 1) ||
        !record_string(package, "newVersion", record->new_version,
                       sizeof(record->new_version), 1) ||
        !record_string(text, "source", record->source, sizeof(record->source),
                       0) ||
        !record_string(text, "signer", record->signer, sizeof(record->signer),
                       1) ||
        !record_string(text, "validation", validation, sizeof(validation), 0) ||
        strcmp(validation, "validated-before-approval") ||
        !record_string(input, "path", record->input, sizeof(record->input),
                       0) ||
        !record_string(input, "blake2b256", record->fingerprint,
                       sizeof(record->fingerprint), 0) ||
        !record_bool(text, "externalStateMayHaveChanged", &record->external) ||
        !record_bool(text, "retryEligible", &retry) || retry ||
        !record_hex(record->plan) || !record_hex(record->fingerprint) ||
        (record->signer[0] && strcmp(record->signer, "unsigned") &&
         !record_hex(record->signer)) ||
        !record_time(record->created) || !record_time(record->updated) ||
        strcmp(record->created, record->updated) > 0 ||
        !wpm_recovery_filters_valid(record->name, record->arch)) {
        return 0;
    }
    if (strcmp(record->operation, "install") &&
        strcmp(record->operation, "remove") &&
        strcmp(record->operation, "upgrade") &&
        strcmp(record->operation, "self-upgrade")) {
        return 0;
    }
    if ((record->old_version[0] &&
         !wpm_repo_version_is_valid(record->old_version)) ||
        (record->new_version[0] &&
         !wpm_repo_version_is_valid(record->new_version)) ||
        (!strcmp(record->operation, "remove") && record->new_version[0]) ||
        (strcmp(record->operation, "remove") && !record->new_version[0]) ||
        !actions || *actions != '[' || !artifacts || *artifacts != '[' ||
        !history ||
        (*history != '[' &&
         (strncmp(history, "null", 4) || !record_delimiter(history + 4))) ||
        !record_string(text, "failedPhase", failed_phase, sizeof(failed_phase),
                       1) ||
        (!strcmp(record->status, "failed") &&
         strcmp(failed_phase, record->phase)) ||
        (strcmp(record->status, "failed") && *failed_phase)) {
        return 0;
    }
    {
        char action[16];
        actions = wpm_json_space(actions + 1);
        if (!wpm_json_read_string(&actions, action, sizeof(action)) ||
            strcmp(action, "inspect") ||
            *(actions = wpm_json_space(actions)) != ']' ||
            !record_delimiter(actions + 1)) {
            return 0;
        }
    }
    if (strcmp(record->status, "completed") &&
        strcmp(record->status, "in-progress") &&
        strcmp(record->status, "failed") && strcmp(record->status, "pending")) {
        return 0;
    }
    if (!strncmp(exit, "null", 4) && record_delimiter(exit + 4)) {
        record->exit_known = 0;
    } else {
        char *end;
        if (*exit < '0' || *exit > '9') {
            return 0;
        }
        errno = 0;
        record->script_exit = strtoul(exit, &end, 10);
        if (errno || record->script_exit > UINT32_MAX ||
            !record_delimiter(end)) {
            return 0;
        }
        record->exit_known = 1;
    }
    record->input_status = "not-checked";
    return record_artifacts(artifacts, record);
}

static int record_read(const char *path, const char *id,
                       recovery_identity *record, size_t *budget) {
    HANDLE file = INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION info;
    char *text = NULL;
    DWORD read;
    int success = 0;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE || GetFileType(file) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(file, &info) || info.nNumberOfLinks != 1 ||
        (info.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        info.nFileSizeHigh || !info.nFileSizeLow ||
        info.nFileSizeLow > RECORD_BYTES) {
        goto done;
    }
    if (info.nFileSizeLow > *budget) {
        success = 2;
        goto done;
    }
    *budget -= info.nFileSizeLow;
    text = malloc((size_t)info.nFileSizeLow + 1);
    if (!text || !ReadFile(file, text, info.nFileSizeLow, &read, NULL) ||
        read != info.nFileSizeLow || memchr(text, 0, read)) {
        goto done;
    }
    text[read] = 0;
    success = record_parse(text, id, record);
done:
    free(text);
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    return success;
}

static int record_validate_input(recovery_identity *record, const char *path,
                                 int allow_unsigned) {
    char canonical[RECORD_PATH], parent[RECORD_PATH], guarded[RECORD_PATH];
    char digest[65], signer[65];
    HANDLE input = INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION info;
    wpm_path_guard *guard = NULL;
    wpm_package_info package;
    DWORD length;
    int success = 0;
    wpm_path_result guarded_result;
    record->input_status = path ? "invalid" : "external-unchecked";
    if (!path) {
        return 0;
    }
    if (!*path || path[strlen(path) - 1] == '.' ||
        path[strlen(path) - 1] == ' ') {
        record->input_status = "unsafe-path";
        return 0;
    }
    if (!strcmp(record->operation, "remove") && !record->old_version[0]) {
        record->input_status = "identity-incomplete";
        return 0;
    }
    length = GetFullPathNameA(path, sizeof(canonical), canonical, NULL);
    if (!length || length >= sizeof(canonical)) {
        goto done;
    }
    if (length < 3 || strchr(canonical + 2, ':') ||
        canonical[length - 1] == '.' || canonical[length - 1] == ' ') {
        record->input_status = "unsafe-path";
        goto done;
    }
    strcpy_s(parent, sizeof(parent), canonical);
    {
        char *separator = strrchr(parent, '\\');
        if (!separator) {
            goto done;
        }
        separator[separator == parent + 2 ? 1 : 0] = 0;
    }
    guarded_result =
        wpm_path_guard_open(parent, 0, guarded, sizeof(guarded), &guard);
    if (guarded_result != WPM_PATH_OPEN) {
        record->input_status = guarded_result == WPM_PATH_ABSENT ? "missing"
                               : guarded_result == WPM_PATH_UNSAFE
                                   ? "unsafe-path"
                                   : "unreadable";
        goto done;
    }
    input = CreateFileA(canonical, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (input == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        record->input_status =
            error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
                ? "missing"
                : "unreadable";
        goto done;
    }
    if (GetFileType(input) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(input, &info) || info.nNumberOfLinks != 1 ||
        (info.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        record->input_status = "unsafe-path";
        goto done;
    }
    if (!wpm_hash_open_file(input, digest, sizeof(digest))) {
        record->input_status = "unreadable";
        goto done;
    }
    if (strcmp(digest, record->fingerprint)) {
        record->input_status = "changed";
        goto done;
    }
    if (!wpm_archive_inspect(canonical, &package) ||
        strcmp(package.name, record->name) ||
        _stricmp(package.arch, record->arch) ||
        strcmp(package.version, !strcmp(record->operation, "remove")
                                    ? record->old_version
                                    : record->new_version)) {
        record->input_status = "identity-conflict";
        goto done;
    }
    if (!wpm_archive_validate_readonly(canonical, allow_unsigned, signer,
                                       sizeof(signer))) {
        record->input_status = "untrusted-or-invalid";
        goto done;
    }
    if (strcmp(signer, record->signer)) {
        record->input_status = "signer-conflict";
        goto done;
    }
    record->input_status = "validated-current-input";
    success = 1;
done:
    if (input != INVALID_HANDLE_VALUE) {
        CloseHandle(input);
    }
    wpm_path_guard_close(guard);
    return success;
}

static int record_retained_path(recovery_identity *record,
                                const char *directory) {
    const char *version = !strcmp(record->operation, "remove")
                              ? record->old_version
                              : record->new_version;
    char relative[256];
    int written;
    if (!record->retained_known) {
        return 1;
    }
    if (!*version) {
        record->retained_status = "identity-incomplete";
        record->inventory_known = 0;
        return 1;
    }
    written = snprintf(relative, sizeof(relative), "%s/input.zip", record->id);
    if (written <= 0 || (size_t)written >= sizeof(relative) ||
        strcmp(relative, record->retained_relative)) {
        return 0;
    }
    written = snprintf(record->retained_path, sizeof(record->retained_path),
                       "%s\\%s\\input.zip", directory, record->id);
    return written > 0 && (size_t)written < sizeof(record->retained_path);
}

static int record_inspect_inputs(recovery_identity *record, const char *archive,
                                 int allow_unsigned) {
    int explicit_ok = 0, retained_ok = 1;
    const char *explicit_status = NULL;
    if (archive) {
        explicit_ok = record_validate_input(record, archive, allow_unsigned);
        explicit_status = record->input_status;
    }
    if (record->retained_known) {
        if (!record->retained_path[0]) {
            record->retained_status = "identity-incomplete";
            retained_ok = 0;
        } else {
            retained_ok = record_validate_input(record, record->retained_path,
                                                allow_unsigned);
            record->retained_status = record->input_status;
        }
    }
    if (archive) {
        record->input_status = explicit_status;
    } else if (!record->retained_known) {
        record->input_status = "external-unchecked";
    } else if (!record->retained_path[0]) {
        record->input_status = "identity-incomplete";
    }
    return record->inventory_known && retained_ok &&
           (archive ? explicit_ok : record->retained_known);
}

static int record_order(const void *left, const void *right) {
    const recovery_identity *a = left, *b = right;
    return strcmp(a->id, b->id);
}

static void record_issue(const char *code, const char *identity) {
    wpm_event_printf(WPM_EVENT_ERROR, code, "recovery", "admit",
                     "Recovery object %s is unreadable or unsupported; "
                     "review schema, path layout, and access while "
                     "preserving evidence.\n",
                     identity);
}

static void record_silent_event(wpm_event_class kind, const char *code,
                                const char *operation, const char *phase,
                                const char *text) {
    (void)kind;
    (void)code;
    (void)operation;
    (void)phase;
    (void)text;
}

static int record_render(const recovery_identity *record, int json) {
    const char *keys[] = {"operationId",
                          "operation",
                          "created",
                          "updated",
                          "planDigest",
                          "status",
                          "phase",
                          "name",
                          "arch",
                          "oldVersion",
                          "newVersion",
                          "source",
                          "inputPath",
                          "blake2b256",
                          "signer",
                          "inputStatus",
                          "retainedInputPath",
                          "retainedInputStatus"};
    const char *values[] = {record->id,
                            record->operation,
                            record->created,
                            record->updated,
                            record->plan,
                            record->status,
                            record->phase,
                            record->name,
                            record->arch,
                            record->old_version[0] ? record->old_version : NULL,
                            record->new_version[0] ? record->new_version : NULL,
                            record->source,
                            record->input,
                            record->fingerprint,
                            record->signer[0] ? record->signer : NULL,
                            record->input_status,
                            record->retained_path[0] ? record->retained_path
                                                     : NULL,
                            record->retained_status};
    int complete = 1;
    if (!json) {
        wpm_event_printf(
            WPM_EVENT_INFO, "recovery.record.item", "recovery", "inspect",
            "  Operation: %s (%s)\n  Package: %s %s %s -> %s\n"
            "  Status/phase: %s / %s\n  Source: %s\n  Input: %s\n"
            "  Input validation: %s\n  Recorded external-state uncertainty: "
            "%s (unauthenticated)\n",
            record->id, record->operation, record->name, record->arch,
            record->old_version[0] ? record->old_version : "unknown",
            record->new_version[0] ? record->new_version : "none",
            record->status, record->phase, record->source, record->input,
            record->input_status, record->external ? "yes" : "no");
        if (record->exit_known) {
            wpm_event_printf(WPM_EVENT_INFO, "recovery.record.exit", "recovery",
                             "inspect", "  Script exit code: %lu\n",
                             record->script_exit);
        }
        return 1;
    }
    fputc('{', stdout);
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        fprintf(stdout, "%s\"%s\":", i ? "," : "", keys[i]);
        complete &= wpm_json_string(values[i]);
    }
    fprintf(stdout, ",\"externalStateMayHaveChanged\":%s,\"scriptExitCode\":",
            record->external ? "true" : "false");
    if (record->exit_known) {
        fprintf(stdout, "%lu", record->script_exit);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"authenticated\":false,\"externalStateKnown\":false,"
          "\"retryEligible\":false,"
          "\"safeActions\":[\"inspect\"],"
          "\"artifactInventoryInterpreted\":",
          stdout);
    fputs(record->inventory_known ? "true}" : "false}", stdout);
    return complete;
}

int wpm_recovery_records(unsigned limit, int json, const char *id,
                         const char *archive, int allow_unsigned,
                         const char *name, const char *arch) {
    char root[RECORD_PATH], supplied[RECORD_PATH], directory[RECORD_PATH];
    char path[RECORD_PATH];
    recovery_identity *records = NULL;
    wpm_path_guard *guard = NULL;
    wpm_path_result guarded;
    WIN32_FIND_DATAA found;
    HANDLE search = INVALID_HANDLE_VALUE;
    unsigned count = 0, examined = 0, invalid = 0, filtered = 0;
    int limited = 0, complete = 0, encoding = 1;
    size_t budget = RECORD_READ_BUDGET;
    wpm_event_sink previous = wpm_event_get_sink();
    if (json && !previous) {
        wpm_event_set_sink(record_silent_event);
    }
    wpm_set_readonly(1);
    if (!limit || limit > 4096 || (id && !wpm_recovery_id_valid(id)) ||
        (archive && !id) || (allow_unsigned && !id) ||
        !wpm_recovery_filters_valid(name, arch)) {
        goto report;
    }
    records = calloc(id ? 1 : limit, sizeof(*records));
    if (!records || !wpm_get_data_root(root, sizeof(root)) ||
        strlen(root) + 10 >= sizeof(supplied)) {
        goto report;
    }
    snprintf(supplied, sizeof(supplied), "%s\\recovery", root);
    guarded =
        wpm_path_guard_open(supplied, 0, directory, sizeof(directory), &guard);
    if (guarded == WPM_PATH_ABSENT) {
        complete = !id;
        goto report;
    }
    if (guarded != WPM_PATH_OPEN || strlen(directory) + 39 >= sizeof(path)) {
        record_issue("recovery.root.unavailable", "data-root/recovery");
        goto report;
    }
    if (id) {
        snprintf(path, sizeof(path), "%s\\%s.json", directory, id);
        examined = 1;
        if (record_read(path, id, records, &budget) != 1 ||
            !record_retained_path(records, directory)) {
            invalid = 1;
            record_issue("recovery.record.invalid", id);
            goto report;
        }
        if ((name && _stricmp(name, records->name)) ||
            (arch && _stricmp(arch, records->arch))) {
            filtered = 1;
            goto report;
        }
        count = 1;
        complete = record_inspect_inputs(records, archive, allow_unsigned);
        goto report;
    }
    snprintf(path, sizeof(path), "%s\\*.json", directory);
    search = FindFirstFileA(path, &found);
    if (search == INVALID_HANDLE_VALUE) {
        complete = GetLastError() == ERROR_FILE_NOT_FOUND;
        goto report;
    }
    do {
        char candidate[33];
        size_t length = strlen(found.cFileName);
        if (examined == limit) {
            limited = 1;
            record_issue("recovery.records.limit", "examined-entry bound");
            break;
        }
        examined++;
        if (length != 37 || strcmp(found.cFileName + 32, ".json")) {
            invalid++;
            record_issue("recovery.record.name.invalid", found.cFileName);
            continue;
        }
        memcpy(candidate, found.cFileName, 32);
        candidate[32] = 0;
        snprintf(path, sizeof(path), "%s\\%s", directory, found.cFileName);
        int admitted =
            wpm_recovery_id_valid(candidate)
                ? record_read(path, candidate, &records[count], &budget)
                : 0;
        if (admitted == 2) {
            limited = 1;
            record_issue("recovery.records.byte-limit",
                         "cumulative-read bound");
            break;
        }
        if (!admitted || !record_retained_path(&records[count], directory)) {
            invalid++;
            record_issue("recovery.record.invalid", candidate);
            memset(&records[count], 0, sizeof(*records));
            continue;
        }
        if ((name && _stricmp(name, records[count].name)) ||
            (arch && _stricmp(arch, records[count].arch))) {
            filtered++;
            memset(&records[count], 0, sizeof(*records));
            continue;
        }
        count++;
    } while (FindNextFileA(search, &found));
    complete = !invalid && !limited && GetLastError() == ERROR_NO_MORE_FILES;
report:
    if (search != INVALID_HANDLE_VALUE) {
        FindClose(search);
    }
    wpm_path_guard_close(guard);
    if (records) {
        qsort(records, count, sizeof(*records), record_order);
    }
    if (json) {
        fputs("{\"schema\":\"wpm.recovery-query.v1\",\"records\":[", stdout);
    } else {
        wpm_event_printf(WPM_EVENT_INFO, "recovery.records.start", "recovery",
                         "inspect",
                         "Recovery records (read-only, "
                         "unauthenticated diagnostic identities):\n");
    }
    for (unsigned i = 0; i < count; i++) {
        if (json && i) {
            fputc(',', stdout);
        }
        encoding &= record_render(&records[i], json);
    }
    complete &= encoding;
    if (json) {
        fprintf(stdout,
                "],\"summary\":{\"examined\":%u,\"records\":%u,"
                "\"invalidOrUnreadable\":%u,\"filteredOut\":%u,\"limited\":%s},"
                "\"complete\":%s,\"exitCode\":%d,\"retrySupported\":false}\n",
                examined, count, invalid, filtered, limited ? "true" : "false",
                complete ? "true" : "false", complete ? 0 : 1);
    } else {
        wpm_event_printf(
            WPM_EVENT_RESULT, "recovery.records.summary", "recovery", "inspect",
            "Result: %u records; %u invalid/unreadable; "
            "limited: %s; complete: %s\n",
            count, invalid, limited ? "yes" : "no", complete ? "yes" : "no");
        wpm_event_printf(
            WPM_EVENT_INFO, "recovery.records.next", "recovery", "inspect",
            "Next action: wpm recovery inspect <operation-id> --archive "
            "<current-archive> [--allow-unsigned]. Inspect current state with "
            "the package maintainer. A validated input does not establish "
            "safe retry or rollback of external script effects.\n");
    }
    if (!complete && !json) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "recovery.records.incomplete", "recovery",
            "inspect",
            "Error: recovery inspection "
            "is incomplete; preserve records and review schema, "
            "access, current input identity, integrity, and trust.\n");
    }
    free(records);
    wpm_event_set_sink(previous);
    return fflush(stdout) == 0 && !ferror(stdout) && complete;
}
