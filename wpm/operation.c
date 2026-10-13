/** @file operation.c @brief Read-only plan construction and guarded execution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include "sodium.h"
#include "archive.h"
#include "logging.h"
#include "operation.h"
#include "capability.h"
#include "json_output.h"
#include "path_guard.h"

#define PLAN_LIMIT 256
#define PLAN_PATH 4096

typedef struct plan_item {
    char action[16], archive[PLAN_PATH], source[PLAN_PATH], old_version[64];
    char signing_key[65];
    wpm_package_info identity;
    unsigned char fingerprint[32];
    HANDLE input;
    DWORD file_identity[3];
    int consumed;
    char recovery_id[33], created[32];
    int journal_started, journal_failed, external;
    char phase[32];
    unsigned long script_exit;
    int script_exit_known;
    DWORD record_identity[3];
} plan_item;

static plan_item *items;
static size_t item_count;
static int collecting, active, assume_yes;
static plan_item *executing;
static unsigned char approved_state[32];
static char selected_source[PLAN_PATH];
static char approved_plan[65];

static void journal_time(char *text, size_t size) {
    SYSTEMTIME now;
    GetSystemTime(&now);
    snprintf(text, size, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", now.wYear,
             now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
             now.wMilliseconds);
}

static int journal_text(const char *text, HANDLE file) {
    size_t size = strlen(text);
    DWORD written;
    return size <= MAXDWORD &&
           WriteFile(file, text, (DWORD)size, &written, NULL) &&
           written == size;
}

static int journal_string(HANDLE file, const char *text) {
    char *encoded = wpm_json_encode_string(text);
    int success = encoded && journal_text(encoded, file);
    free(encoded);
    return success;
}

static int journal_format(HANDLE file, const char *format, ...) {
    char text[256];
    va_list arguments;
    int size;
    va_start(arguments, format);
    size = vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    return size >= 0 && (size_t)size < sizeof(text) && journal_text(text, file);
}

static int journal_write(plan_item *item, const char *phase, const char *status,
                         const unsigned long *exit_code) {
    char root[PLAN_PATH], supplied[PLAN_PATH], directory[PLAN_PATH];
    char final[PLAN_PATH];
    char temporary[PLAN_PATH] = "", updated[32], fingerprint[65];
    HANDLE file = INVALID_HANDLE_VALUE;
    wpm_path_guard *guard = NULL;
    int complete = 1, success = 0;
    unsigned char random[16];
    DWORD length;
    BY_HANDLE_FILE_INFORMATION committed;
    if (exit_code) {
        item->script_exit = *exit_code;
        item->script_exit_known = 1;
    }
    if (item->script_exit_known) {
        exit_code = &item->script_exit;
    }
    if (item->journal_failed || wpm_is_readonly() ||
        !wpm_get_data_root(supplied, sizeof(supplied))) {
        goto done;
    }
    length = GetFullPathNameA(supplied, sizeof(root), root, NULL);
    if (!length || length <= 3 || length >= sizeof(root) ||
        strlen(root) + 10 >= sizeof(directory)) {
        goto done;
    }
    snprintf(directory, sizeof(directory), "%s\\recovery", root);
    if (wpm_path_guard_open(directory, 1, supplied, sizeof(supplied), &guard) !=
        WPM_PATH_OPEN) {
        goto done;
    }
    if (!item->recovery_id[0]) {
        randombytes_buf(random, sizeof(random));
        sodium_bin2hex(item->recovery_id, sizeof(item->recovery_id), random,
                       sizeof(random));
        journal_time(item->created, sizeof(item->created));
    }
    if (snprintf(final, sizeof(final), "%s\\%s.json", directory,
                 item->recovery_id) < 0 ||
        strlen(directory) + 43 >= sizeof(final)) {
        goto done;
    }
    snprintf(temporary, sizeof(temporary), "%s.new", final);
    if (!item->journal_started &&
        GetFileAttributesA(final) != INVALID_FILE_ATTRIBUTES) {
        goto done;
    }
    if (item->journal_started) {
        BY_HANDLE_FILE_INFORMATION previous;
        HANDLE existing = CreateFileA(
            final, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        int same = existing != INVALID_HANDLE_VALUE &&
                   GetFileInformationByHandle(existing, &previous) &&
                   previous.nNumberOfLinks == 1 &&
                   !(previous.dwFileAttributes &
                     (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY |
                      FILE_ATTRIBUTE_READONLY)) &&
                   previous.dwVolumeSerialNumber == item->record_identity[0] &&
                   previous.nFileIndexHigh == item->record_identity[1] &&
                   previous.nFileIndexLow == item->record_identity[2];
        if (existing != INVALID_HANDLE_VALUE) {
            CloseHandle(existing);
        }
        if (!same) {
            goto done;
        }
    }
    file =
        CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        goto done;
    }
    journal_time(updated, sizeof(updated));
    sodium_bin2hex(fingerprint, sizeof(fingerprint), item->fingerprint,
                   sizeof(item->fingerprint));
    complete &=
        journal_text("{\"schema\":\"wpm.recovery.v1\",\"operationId\":", file);
    complete &= journal_string(file, item->recovery_id);
    complete &= journal_text(",\"operation\":", file);
    complete &= journal_string(file, item->action);
    complete &= journal_text(",\"created\":", file);
    complete &= journal_string(file, item->created);
    complete &= journal_text(",\"updated\":", file);
    complete &= journal_string(file, updated);
    complete &= journal_text(",\"planDigest\":", file);
    complete &= journal_string(file, approved_plan);
    complete &= journal_text(",\"status\":", file);
    complete &= journal_string(file, status);
    complete &= journal_text(",\"phase\":", file);
    complete &= journal_string(file, phase);
    complete &= journal_text(",\"package\":{\"name\":", file);
    complete &= journal_string(file, item->identity.name);
    complete &= journal_text(",\"arch\":", file);
    complete &= journal_string(file, item->identity.arch);
    complete &= journal_text(",\"oldVersion\":", file);
    complete &= journal_string(
        file, !strcmp(item->action, "remove")      ? item->identity.version
              : !strcmp(item->old_version, "none") ? NULL
                                                   : item->old_version);
    complete &= journal_text(",\"newVersion\":", file);
    complete &= journal_string(
        file, !strcmp(item->action, "remove") ? NULL : item->identity.version);
    complete &= journal_text("},\"source\":", file);
    complete &= journal_string(file, item->source);
    complete &= journal_text(",\"signer\":", file);
    complete &=
        journal_string(file, item->signing_key[0] ? item->signing_key : NULL);
    complete &=
        journal_text(",\"validation\":\"validated-before-approval\",\"input\":{"
                     "\"path\":",
                     file);
    complete &= journal_string(file, item->archive);
    complete &= journal_text(",\"blake2b256\":", file);
    complete &= journal_string(file, fingerprint);
    complete &= journal_format(
        file, "},\"externalStateMayHaveChanged\":%s,\"scriptExitCode\":",
        item->external ? "true" : "false");
    if (exit_code) {
        complete &= journal_format(file, "%lu", *exit_code);
    } else {
        complete &= journal_text("null", file);
    }
    complete &=
        journal_text(",\"retryEligible\":false,\"safeActions\":[\"inspect\"],"
                     "\"controlledArtifacts\":[],\"completedPhases\":null,"
                     "\"failedPhase\":",
                     file);
    complete &= journal_string(file, !strcmp(status, "failed") ? phase : NULL);
    complete &= journal_text("}\n", file);
    complete &= FlushFileBuffers(file) != 0;
    complete &= GetFileInformationByHandle(file, &committed) != 0;
    complete &= CloseHandle(file) != 0;
    file = INVALID_HANDLE_VALUE;
    if (!complete ||
        !MoveFileExA(
            temporary, final,
            MOVEFILE_WRITE_THROUGH |
                (item->journal_started ? MOVEFILE_REPLACE_EXISTING : 0))) {
        goto done;
    }
    item->journal_started = 1;
    item->record_identity[0] = committed.dwVolumeSerialNumber;
    item->record_identity[1] = committed.nFileIndexHigh;
    item->record_identity[2] = committed.nFileIndexLow;
    strcpy_s(item->phase, sizeof(item->phase), phase);
    success = 1;
done:
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    /* Preserve failed temporary evidence. Never delete a name after releasing
     * its handle: it may now identify an unrelated substituted object. */
    wpm_path_guard_close(guard);
    if (!success) {
        item->journal_failed = 1;
        wpm_event_printf(WPM_EVENT_ERROR, "recovery.record.failed", "recovery",
                         "record",
                         "Error: recovery evidence could not be "
                         "committed; further planned mutation is blocked.\n");
    }
    return success;
}

static int hash_file(const char *path, unsigned char digest[32]) {
    HANDLE file =
        CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    crypto_generichash_state hash;
    unsigned char buffer[65536];
    DWORD count;
    int success = 0;
    if (file == INVALID_HANDLE_VALUE || sodium_init() < 0) {
        goto done;
    }
    if (crypto_generichash_init(&hash, NULL, 0, 32)) {
        goto done;
    }
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &count, NULL)) {
            goto done;
        }
        if (!count) {
            break;
        }
        if (crypto_generichash_update(&hash, buffer, count)) {
            goto done;
        }
    }
    success = crypto_generichash_final(&hash, digest, 32) == 0;
done:
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    return success;
}

static int read_file_identity(HANDLE input, DWORD identity[3]) {
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(input, &info)) {
        return 0;
    }
    identity[0] = info.dwVolumeSerialNumber;
    identity[1] = info.nFileIndexHigh;
    identity[2] = info.nFileIndexLow;
    return 1;
}
static int file_identity_matches(const plan_item *item) {
    DWORD identity[3];
    HANDLE input = CreateFileA(item->archive, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    int matches = input != INVALID_HANDLE_VALUE &&
                  read_file_identity(input, identity) &&
                  !memcmp(identity, item->file_identity, sizeof(identity));
    if (input != INVALID_HANDLE_VALUE) {
        CloseHandle(input);
    }
    return matches;
}
static int compare_names(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

static int hash_tree(crypto_generichash_state *hash, const char *path,
                     int depth) {
    char pattern[PLAN_PATH], child[PLAN_PATH];
    WIN32_FIND_DATAA data;
    HANDLE find;
    char **names = NULL;
    size_t count = 0;
    int result = 0;
    DWORD error;
    DWORD root_attributes = GetFileAttributesA(path);
    if (root_attributes != INVALID_FILE_ATTRIBUTES &&
        (root_attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return 0;
    }
    if (depth > 8 || snprintf(pattern, sizeof(pattern), "%s\\*", path) >=
                         (int)sizeof(pattern)) {
        return 0;
    }
    find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    do {
        char **grown;
        if (!strcmp(data.cFileName, ".") || !strcmp(data.cFileName, "..")) {
            continue;
        }
        if (count >= 8192 ||
            (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            goto done;
        }
        grown = realloc(names, (count + 1) * sizeof(char *));
        if (!grown) {
            goto done;
        }
        names = grown;
        names[count] = malloc(strlen(data.cFileName) + 1);
        if (!names[count]) {
            goto done;
        }
        strcpy(names[count++], data.cFileName);
    } while (FindNextFileA(find, &data));
    if (GetLastError() != ERROR_NO_MORE_FILES) {
        goto done;
    }
    if (count > 1) {
        qsort(names, count, sizeof(char *), compare_names);
    }
    for (size_t i = 0; i < count; i++) {
        unsigned char digest[32];
        DWORD attributes;
        if (snprintf(child, sizeof(child), "%s\\%s", path, names[i]) >=
            (int)sizeof(child)) {
            goto done;
        }
        attributes = GetFileAttributesA(child);
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
            crypto_generichash_update(hash, (unsigned char *)child,
                                      strlen(child) + 1)) {
            goto done;
        }
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!hash_tree(hash, child, depth + 1)) {
                goto done;
            }
        } else {
            if (!hash_file(child, digest) ||
                crypto_generichash_update(hash, digest, sizeof(digest))) {
                goto done;
            }
        }
    }
    result = 1;
done:
    CloseHandle(find);
    for (size_t i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
    return result;
}

static int state_digest(unsigned char digest[32]) {
    const char *roots[] = {"packages", "trust", "config"};
    char root[PLAN_PATH], path[PLAN_PATH];
    crypto_generichash_state hash;
    if (!wpm_get_data_root(root, sizeof(root)) || sodium_init() < 0 ||
        crypto_generichash_init(&hash, NULL, 0, 32)) {
        return 0;
    }
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        if (snprintf(path, sizeof(path), "%s\\%s", root, roots[i]) >=
                (int)sizeof(path) ||
            crypto_generichash_update(&hash, (unsigned char *)path,
                                      strlen(path) + 1) ||
            !hash_tree(&hash, path, 0)) {
            return 0;
        }
    }
    return crypto_generichash_final(&hash, digest, 32) == 0;
}

void wpm_operation_close(void) {
    if (executing && executing->journal_started && !executing->journal_failed) {
        journal_write(executing, executing->phase, "failed", NULL);
    }
    for (size_t i = 0; i < item_count; i++) {
        if (items[i].input != INVALID_HANDLE_VALUE) {
            CloseHandle(items[i].input);
        }
    }
    free(items);
    items = NULL;
    item_count = 0;
    active = 0;
    collecting = 0;
    executing = NULL;
}

void wpm_operation_begin(int yes) {
    wpm_operation_close();
    active = 1;
    collecting = 1;
    assume_yes = yes != 0;
    atexit(wpm_operation_close);
}

int wpm_operation_collecting(void) { return active && collecting; }

int wpm_operation_active(void) { return active; }
void wpm_operation_set_source(const char *source) {
    selected_source[0] = 0;
    if (source) {
        strcpy_s(selected_source, sizeof(selected_source), source);
    }
}

static int changed(void) {
    wpm_event_printf(WPM_EVENT_ERROR, "plan.changed", "plan", "authorize",
                     "Error: operation plan inputs or controlled state "
                     "changed; recompute and approve a new plan.\n");
    return 0;
}

int wpm_operation_prepare(const char *source, const char *destination) {
    unsigned char initial[32], current[32], digest[32];
    crypto_generichash_state hash;
    char answer[32], hex[65];
    if (!wpm_require_capability(WPM_CAP_WRITE, "cache preparation")) {
        return 0;
    }
    if (!active) {
        return 1;
    }
    if (!collecting || !state_digest(initial) ||
        crypto_generichash_init(&hash, NULL, 0, sizeof(digest))) {
        return changed();
    }
    crypto_generichash_update(&hash, (unsigned char *)source,
                              strlen(source) + 1);
    crypto_generichash_update(&hash, (unsigned char *)destination,
                              strlen(destination) + 1);
    crypto_generichash_update(&hash, initial, sizeof(initial));
    crypto_generichash_final(&hash, digest, sizeof(digest));
    sodium_bin2hex(hex, sizeof(hex), digest, sizeof(digest));
    wpm_event_printf(
        WPM_EVENT_INFO, "plan.prepare", "plan", "preview",
        "Cache preparation plan:\n  Source: %s\n  Destination: %s\n"
        "  Effects: create WPM data directories and retrieve this cache "
        "input.\n"
        "  Scripts/handoff: none; full deployment approval follows "
        "validation.\n"
        "Preparation digest: %s\n",
        source, destination, hex);
    if (!assume_yes) {
        wpm_event_printf(WPM_EVENT_PROMPT, "plan.prepare.confirm", "plan",
                         "confirm", "Prepare this cache input? [y/N]: ");
        fflush(stdout);
        if (!fgets(answer, sizeof(answer), stdin)) {
            return 0;
        }
        answer[strcspn(answer, "\r\n")] = 0;
        if (_stricmp(answer, "y") && _stricmp(answer, "yes")) {
            return 0;
        }
    }
    if (!state_digest(current) || memcmp(initial, current, sizeof(initial))) {
        return changed();
    }
    return 1;
}

int wpm_operation_archive(const char *action, const char *archive,
                          const char *old_version, const char *source,
                          int allow_unsigned) {
    char full[PLAN_PATH];
    unsigned char fingerprint[32], state[32];
    DWORD length;
    if (!active) {
        return 1; /* Legacy internal handoff validates independently. */
    }
    if (executing) {
        /* A previous item has not committed completion. Keep its evidence
         * and stop the approved batch rather than replacing its identity. */
        return changed();
    }
    if (selected_source[0]) {
        source = selected_source;
    }
    length = GetFullPathNameA(archive, sizeof(full), full, NULL);
    if (!length || length >= sizeof(full) || !hash_file(full, fingerprint)) {
        return changed();
    }
    if (collecting) {
        plan_item item, *grown;
        for (size_t i = 0; i < item_count; i++) {
            if (!_stricmp(items[i].archive, full) &&
                !strcmp(items[i].action, action)) {
                return changed();
            }
        }
        if (item_count >= PLAN_LIMIT) {
            return changed();
        }
        memset(&item, 0, sizeof(item));
        item.input = INVALID_HANDLE_VALUE;
        if (!wpm_archive_inspect(full, &item.identity) ||
            !wpm_archive_validate_readonly(full, allow_unsigned,
                                           item.signing_key,
                                           sizeof(item.signing_key))) {
            return 0;
        }
        strcpy_s(item.action, sizeof(item.action), action);
        strcpy_s(item.archive, sizeof(item.archive), full);
        strcpy_s(item.source, sizeof(item.source),
                 source && *source ? source : full);
        strcpy_s(item.old_version, sizeof(item.old_version),
                 old_version ? old_version : "none");
        memcpy(item.fingerprint, fingerprint, sizeof(fingerprint));
        item.input =
            CreateFileA(full, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (item.input == INVALID_HANDLE_VALUE) {
            return changed();
        }
        if (!read_file_identity(item.input, item.file_identity)) {
            CloseHandle(item.input);
            return changed();
        }
        grown = realloc(items, (item_count + 1) * sizeof(*items));
        if (!grown) {
            CloseHandle(item.input);
            return changed();
        }
        items = grown;
        items[item_count++] = item;
        return 1;
    }
    if (!state_digest(state) || memcmp(state, approved_state, sizeof(state))) {
        return changed();
    }
    for (size_t i = 0; i < item_count; i++) {
        plan_item *item = &items[i];
        if (!item->consumed && !strcmp(item->action, action) &&
            !_stricmp(item->archive, full) &&
            !strcmp(item->source, source && *source ? source : full) &&
            !strcmp(item->old_version, old_version ? old_version : "none") &&
            !memcmp(item->fingerprint, fingerprint, sizeof(fingerprint)) &&
            file_identity_matches(item)) {
            item->consumed = 1;
            executing = item;
            return journal_write(item, "prepare", "in-progress", NULL);
        }
    }
    return changed();
}

int wpm_operation_confirm(void) {
    crypto_generichash_state hash;
    unsigned char digest[32], current[32];
    char hex[65], root[PLAN_PATH], answer[32];
    if (!active || !collecting || !state_digest(approved_state) ||
        !wpm_get_data_root(root, sizeof(root)) ||
        crypto_generichash_init(&hash, NULL, 0, sizeof(digest))) {
        changed();
        return -1;
    }
    wpm_event_printf(WPM_EVENT_INFO, "plan.start", "plan", "preview",
                     "Operation plan:\n");
    for (size_t i = 0; i < item_count; i++) {
        plan_item *item = &items[i];
        const char *basename = strrchr(item->archive, '\\');
        basename = basename ? basename + 1 : item->archive;
        const char *fields[] = {item->action,           item->archive,
                                item->source,           item->old_version,
                                item->identity.name,    item->identity.arch,
                                item->identity.version, item->signing_key};
        for (size_t j = 0; j < sizeof(fields) / sizeof(fields[0]); j++) {
            crypto_generichash_update(&hash, (unsigned char *)fields[j],
                                      strlen(fields[j]) + 1);
        }
        crypto_generichash_update(&hash, item->fingerprint,
                                  sizeof(item->fingerprint));
        crypto_generichash_update(&hash, (unsigned char *)item->file_identity,
                                  sizeof(item->file_identity));
        wpm_event_printf(
            WPM_EVENT_INFO, "plan.item", "plan", "preview",
            "  %s %s %s %s -> %s\n  Source: %s\n  Input: %s\n  Trust: %s\n"
            "  Controlled roots: %s\\packages; %s\\temp; %s\\audit\n"
            "  Script: .wpm/%s.cmd (may change deployment outside WPM's "
            "control)\n"
            "  Retained archive: %s\\packages\\%s\n"
            "  Archive: %s\n  Completion: %s\n",
            item->action, item->identity.name, item->identity.arch,
            !strcmp(item->action, "remove") ? item->identity.version
                                            : item->old_version,
            !strcmp(item->action, "remove") ? "removed"
                                            : item->identity.version,
            item->source, item->archive, item->signing_key, root, root, root,
            !strcmp(item->action, "remove") ? "remove" : "install", root,
            basename,
            !strcmp(item->action, "remove")
                ? "remove selected retained archive"
                : "retain new archive and existing versions",
            !strcmp(item->action, "self-upgrade")
                ? "scheduled after this process exits"
                : "installed/removed after script success");
    }
    crypto_generichash_update(&hash, approved_state, sizeof(approved_state));
    crypto_generichash_final(&hash, digest, sizeof(digest));
    sodium_bin2hex(hex, sizeof(hex), digest, sizeof(digest));
    strcpy_s(approved_plan, sizeof(approved_plan), hex);
    wpm_event_printf(WPM_EVENT_INFO, "plan.digest", "plan", "preview",
                     "Plan digest: %s\n", hex);
    if (wpm_is_readonly()) {
        wpm_event_printf(WPM_EVENT_RESULT, "plan.dry-run", "plan", "preview",
                         "Result: dry-run planned; no writes, scripts, or "
                         "handoff authorized.\n");
        return 0;
    }
    if (item_count && !assume_yes) {
        wpm_event_printf(WPM_EVENT_PROMPT, "plan.confirm", "plan", "confirm",
                         "Proceed with these changes? [y/N]: ");
        fflush(stdout);
        if (!fgets(answer, sizeof(answer), stdin)) {
            goto cancelled;
        }
        answer[strcspn(answer, "\r\n")] = 0;
        if (_stricmp(answer, "y") && _stricmp(answer, "yes")) {
            goto cancelled;
        }
    }
    if (!state_digest(current) ||
        memcmp(current, approved_state, sizeof(current))) {
        changed();
        return -1;
    }
    for (size_t i = 0; i < item_count; i++) {
        unsigned char fingerprint[32];
        if (!file_identity_matches(&items[i]) ||
            !hash_file(items[i].archive, fingerprint) ||
            memcmp(fingerprint, items[i].fingerprint, sizeof(fingerprint))) {
            changed();
            return -1;
        }
    }
    collecting = 0;
    return 1;
cancelled:
    wpm_event_printf(WPM_EVENT_INFO, "plan.cancelled", "plan", "confirm",
                     "Operation cancelled; no package scripts were run or "
                     "deployments changed.\n");
    return 0;
}

int wpm_operation_before_script(void) {
    if (!wpm_require_capability(WPM_CAP_SCRIPT, "package script execution")) {
        return 0;
    }
    unsigned char digest[32], state[32];
    if (!active) {
        return 1;
    }
    if (collecting || !executing || !file_identity_matches(executing) ||
        !hash_file(executing->archive, digest) ||
        memcmp(digest, executing->fingerprint, sizeof(digest)) ||
        !state_digest(state) || memcmp(state, approved_state, sizeof(state))) {
        return changed();
    }
    return !executing->journal_failed;
}

int wpm_operation_external_start(const char *phase) {
    if (!active) {
        return 1;
    }
    if (!executing || collecting) {
        return 0;
    }
    executing->external = 1;
    executing->script_exit_known = 0;
    return journal_write(executing, phase, "in-progress", NULL);
}

int wpm_operation_script_result(unsigned long exit_code) {
    if (!active) {
        return 1;
    }
    return executing &&
           journal_write(executing, "script",
                         exit_code ? "failed" : "in-progress", &exit_code);
}

int wpm_operation_completed(void) {
    if (active && !collecting) {
        if (!executing ||
            !journal_write(
                executing,
                !strcmp(executing->action, "self-upgrade") ? "handoff"
                                                           : "completed",
                !strcmp(executing->action, "self-upgrade") ? "pending"
                                                           : "completed",
                NULL)) {
            return 0;
        }
        if (!state_digest(approved_state)) {
            memset(approved_state, 0, sizeof(approved_state));
        }
        executing = NULL;
    }
    return 1;
}
