/** @file operation.c @brief Read-only plan construction and guarded execution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "sodium.h"
#include "archive.h"
#include "logging.h"
#include "operation.h"
#include "capability.h"

#define PLAN_LIMIT 256
#define PLAN_PATH 4096

typedef struct plan_item {
    char action[16], archive[PLAN_PATH], source[PLAN_PATH], old_version[64];
    char signing_key[65];
    wpm_package_info identity;
    unsigned char fingerprint[32];
    HANDLE input;
    int consumed;
} plan_item;

static plan_item *items;
static size_t item_count;
static int collecting, active, assume_yes;
static plan_item *executing;
static unsigned char approved_state[32];
static char selected_source[PLAN_PATH];

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
            !memcmp(item->fingerprint, fingerprint, sizeof(fingerprint))) {
            item->consumed = 1;
            executing = item;
            return 1;
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
        if (!hash_file(items[i].archive, fingerprint) ||
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
    if (collecting || !executing || !hash_file(executing->archive, digest) ||
        memcmp(digest, executing->fingerprint, sizeof(digest)) ||
        !state_digest(state) || memcmp(state, approved_state, sizeof(state))) {
        return changed();
    }
    return 1;
}

void wpm_operation_completed(void) {
    if (active && !collecting) {
        if (!state_digest(approved_state)) {
            memset(approved_state, 0, sizeof(approved_state));
        }
        executing = NULL;
    }
}
