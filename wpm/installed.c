/** @file installed.c @brief Protected, atomic, payload-free installation
 * records. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sodium.h"
#ifdef __TINYC__
#include "tcc_support/aclapi.h"
#else
#include <aclapi.h>
#endif
#include "installed.h"
#include "path_guard.h"
#include "json_input.h"
#include "capability.h"
#include "operation.h"
#include "logging.h"

#define RECORD_PATH 4096
#define RECORD_SIZE 32768

static int basename_safe(const char *name) {
    size_t length = name ? strlen(name) : 0;
    if (!length || length > 255 || !strcmp(name, ".") || !strcmp(name, "..") ||
        name[length - 1] == '.' || name[length - 1] == ' ') {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (*p < 32 || *p == 127 || strchr("\\/:\"<>|*?", *p)) {
            return 0;
        }
    }
    return 1;
}

/* Keep publication names ASCII even when the retained filename is not. */
static int record_name(const char *archive, char *name) {
    unsigned char key[256], digest[crypto_hash_sha256_BYTES];
    if (!basename_safe(archive)) {
        return 0;
    }
    size_t length = strlen(archive);
    for (size_t i = 0; i < length; i++) {
        key[i] = (unsigned char)tolower((unsigned char)archive[i]);
    }
    crypto_hash_sha256(digest, key, length);
    sodium_bin2hex(name, 2 * sizeof(digest) + 1, digest, sizeof(digest));
    strcat(name, ".json");
    return 1;
}

static wpm_path_result record_root(int create, char *canonical,
                                   wpm_path_guard **guard) {
    char root[RECORD_PATH], path[RECORD_PATH];
    if (!wpm_get_data_root(root, sizeof(root)) ||
        snprintf(path, sizeof(path), "%s\\installed", root) >= sizeof(path)) {
        return WPM_PATH_FAILED;
    }
    return wpm_path_guard_open(path, create, canonical, RECORD_PATH, guard);
}

/* Owner, administrators and SYSTEM can write; other local users can read.
 * Protected inheritance prevents a writable cache/data ACL reaching records. */
static int protect_records(const char *path) {
    HANDLE token = NULL;
    PTOKEN_USER user = NULL;
    DWORD size = 0;
    PACL acl = NULL;
    PSID admins = NULL, system = NULL, users = NULL;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    EXPLICIT_ACCESSA access[4];
    int ok = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        goto done;
    }
    GetTokenInformation(token, TokenUser, NULL, 0, &size);
    user = malloc(size);
    if (!user || !GetTokenInformation(token, TokenUser, user, size, &size) ||
        !AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &admins) ||
        !AllocateAndInitializeSid(&authority, 1, SECURITY_LOCAL_SYSTEM_RID, 0,
                                  0, 0, 0, 0, 0, 0, &system) ||
        !AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0,
                                  &users)) {
        goto done;
    }
    PSID sids[4] = {user->User.Sid, admins, system, users};
    memset(access, 0, sizeof(access));
    for (int i = 0; i < 4; i++) {
        access[i].grfAccessPermissions =
            i == 3 ? GENERIC_READ | GENERIC_EXECUTE : GENERIC_ALL;
        access[i].grfAccessMode = SET_ACCESS;
        access[i].grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
        access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access[i].Trustee.ptstrName = (LPSTR)sids[i];
    }
    if (SetEntriesInAclA(4, access, NULL, &acl) != ERROR_SUCCESS) {
        goto done;
    }
    ok = SetNamedSecurityInfoA((LPSTR)path, SE_FILE_OBJECT,
                               DACL_SECURITY_INFORMATION |
                                   PROTECTED_DACL_SECURITY_INFORMATION,
                               NULL, NULL, acl, NULL) == ERROR_SUCCESS;
done:
    if (acl) {
        LocalFree(acl);
    }
    if (admins) {
        FreeSid(admins);
    }
    if (system) {
        FreeSid(system);
    }
    if (users) {
        FreeSid(users);
    }
    free(user);
    if (token) {
        CloseHandle(token);
    }
    return ok;
}

static int field(const char *text, const char *name, char *value, size_t size) {
    const char *p = wpm_json_unique_field(text, name);
    return p && wpm_json_read_string(&p, value, size);
}

static int read_record(wpm_path_guard *guard, const char *name,
                       wpm_package_info *info) {
    HANDLE file = wpm_path_guard_file(guard, name, 0);
    char *text = malloc(RECORD_SIZE);
    DWORD size, high, read;
    int ok = 0;
    memset(info, 0, sizeof(*info));
    if (!file || !text) {
        goto done;
    }
    size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high || size < 2 || size >= RECORD_SIZE ||
        !ReadFile(file, text, size, &read, NULL) || read != size ||
        memchr(text, 0, size)) {
        goto done;
    }
    text[size] = 0;
    if (!wpm_json_document_valid(text) || !wpm_json_version_one(text) ||
        !field(text, "name", info->name, sizeof(info->name)) ||
        !field(text, "version_string", info->version, sizeof(info->version)) ||
        !field(text, "arch", info->arch, sizeof(info->arch)) ||
        !field(text, "archive", info->archive_name,
               sizeof(info->archive_name)) ||
        !basename_safe(info->name) || !basename_safe(info->archive_name) ||
        !basename_safe(info->version) ||
        (_stricmp(info->arch, "any") && _stricmp(info->arch, "x86") &&
         _stricmp(info->arch, "x64") && _stricmp(info->arch, "arm64"))) {
        goto done;
    }
    char expected[RECORD_PATH];
    if (!record_name(info->archive_name, expected) ||
        _stricmp(expected, name)) {
        goto done;
    }
    for (int i = 0; i < WPM_METADATA_FIELDS; i++) {
        const char *p = wpm_json_unique_field(text, wpm_metadata_names[i]);
        char value[WPM_METADATA_SIZE];
        if (p && (!wpm_json_read_string(&p, value, sizeof(value)) ||
                  !wpm_metadata_set(&info->optional, i, value))) {
            goto done;
        }
    }
    ok = 1;
done:
    if (file) {
        CloseHandle(file);
    }
    free(text);
    return ok;
}

static int save_record(const wpm_package_info *info, int replace) {
    char canonical[RECORD_PATH], name[RECORD_PATH], temporary[128];
    wpm_path_guard *guard = NULL;
    HANDLE file = NULL;
    char extra[WPM_METADATA_JSON_SIZE], text[RECORD_SIZE];
    int size;
    DWORD written;
    int ok = 0;
    if (!wpm_require_capability(WPM_CAP_WRITE, "save installed record") ||
        !record_name(info->archive_name, name) || !basename_safe(info->name)) {
        return 0;
    }
    if (record_root(1, canonical, &guard) != WPM_PATH_OPEN ||
        !protect_records(canonical)) {
        goto done;
    }
    snprintf(temporary, sizeof(temporary), "pending-%lu-%lu.tmp",
             (unsigned long)GetCurrentProcessId(),
             (unsigned long)GetTickCount());
    file = wpm_path_guard_file(guard, temporary, 1);
    if (!file || !basename_safe(info->version) || !basename_safe(info->arch) ||
        !wpm_metadata_json(&info->optional, extra, sizeof(extra))) {
        goto done;
    }
    size = snprintf(text, sizeof(text),
                    "{\"version\":1,\"name\":\"%s\",\"version_string\":\"%s\","
                    "\"arch\":\"%s\",\"archive\":\"%s\"%s}\n",
                    info->name, info->version, info->arch, info->archive_name,
                    extra);
    if (size < 0 || (size_t)size >= sizeof(text) ||
        !WriteFile(file, text, (DWORD)size, &written, NULL) ||
        written != (DWORD)size || !FlushFileBuffers(file) ||
        !wpm_path_guard_publish(guard, file, name, replace)) {
        goto done;
    }
    ok = 1;
done:
    if (!ok) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "installed.write.failed", "installed", "record",
            "Error: could not persist installation record for %s.\n",
            info->name);
        if (file) {
            wpm_path_guard_discard_fresh(guard, file);
        }
    }
    if (file) {
        CloseHandle(file);
    }
    wpm_path_guard_close(guard);
    return ok;
}

int wpm_installed_save(const wpm_package_info *info) {
    return save_record(info, 1);
}

int wpm_installed_read(wpm_package_info *items, int capacity, int *count,
                       int *issues, int *limited) {
    char canonical[RECORD_PATH], name[256], root[RECORD_PATH],
        path[RECORD_PATH];
    wpm_path_guard *guard = NULL;
    WIN32_FIND_DATAA found;
    HANDLE search = INVALID_HANDLE_VALUE;
    int next, restart = 1, ok = 0, scanned = 0;
    *count = *issues = *limited = 0;
    wpm_path_result state = record_root(0, canonical, &guard);
    if (state != WPM_PATH_OPEN && state != WPM_PATH_ABSENT) {
        goto done;
    }
    if (state == WPM_PATH_OPEN) {
        while ((next = wpm_path_guard_next_json(guard, name, sizeof(name),
                                                restart)) > 0) {
            restart = 0;
            if (scanned++ == capacity) {
                *limited = 1;
                break;
            }
            if (read_record(guard, name, &items[*count])) {
                (*count)++;
            } else {
                (*issues)++;
            }
        }
        if (next < 0) {
            (*issues)++;
        }
    }
    /* Legacy fallback is read-only. Never infer a missing archive is removal.
     */
    if (!wpm_get_data_root(root, sizeof(root)) ||
        snprintf(path, sizeof(path), "%s\\packages", root) >= sizeof(path)) {
        goto done;
    }
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        ok = GetLastError() == ERROR_FILE_NOT_FOUND ||
             GetLastError() == ERROR_PATH_NOT_FOUND;
        goto done;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        goto done;
    }
    snprintf(path, sizeof(path), "%s\\packages\\*.zip", root);
    search = FindFirstFileA(path, &found);
    if (search == INVALID_HANDLE_VALUE) {
        ok = GetLastError() == ERROR_FILE_NOT_FOUND;
        goto done;
    }
    do {
        int duplicate = 0;
        for (int i = 0; i < *count; i++) {
            if (!_stricmp(items[i].archive_name, found.cFileName)) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        if (scanned++ == capacity) {
            *limited = 1;
            break;
        }
        if ((found.dwFileAttributes &
             (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
            snprintf(path, sizeof(path), "%s\\packages\\%s", root,
                     found.cFileName) >= sizeof(path)) {
            (*issues)++;
            continue;
        }
        HANDLE input =
            CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        if (input == INVALID_HANDLE_VALUE) {
            (*issues)++;
            continue;
        }
        BY_HANDLE_FILE_INFORMATION attributes;
        int valid =
            GetFileInformationByHandle(input, &attributes) &&
            !(attributes.dwFileAttributes &
              (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
            wpm_archive_inspect_open(input, &items[*count]);
        CloseHandle(input);
        if (!valid || !basename_safe(items[*count].name)) {
            (*issues)++;
            continue;
        }
        strcpy_s(items[*count].archive_name, sizeof(items[*count].archive_name),
                 found.cFileName);
        (*count)++;
    } while (FindNextFileA(search, &found));
    ok = *limited || GetLastError() == ERROR_NO_MORE_FILES;
done:
    if (search != INVALID_HANDLE_VALUE) {
        FindClose(search);
    }
    wpm_path_guard_close(guard);
    return ok;
}

int wpm_installed_migrate(void) {
    int count, issues, limited, ok;
    if (wpm_is_readonly() || wpm_operation_active()) {
        return 1;
    }
    wpm_package_info *items = calloc(256, sizeof(*items));
    if (!items) {
        return 0;
    }
    ok = wpm_installed_read(items, 256, &count, &issues, &limited);
    if (ok) {
        for (int i = 0; i < count; i++) {
            char canonical[RECORD_PATH], name[RECORD_PATH];
            wpm_path_guard *guard = NULL;
            if (!record_name(items[i].archive_name, name)) {
                ok = 0;
                break;
            }
            wpm_path_result state = record_root(0, canonical, &guard);
            HANDLE file = state == WPM_PATH_OPEN
                              ? wpm_path_guard_file(guard, name, 0)
                              : NULL;
            int exists = file != NULL;
            if (file) {
                CloseHandle(file);
            }
            wpm_path_guard_close(guard);
            if (!exists) {
                char root[RECORD_PATH], archive[RECORD_PATH];
                if (!wpm_get_data_root(root, sizeof(root)) ||
                    snprintf(archive, sizeof(archive), "%s\\packages\\%s", root,
                             items[i].archive_name) >= sizeof(archive)) {
                    ok = 0;
                    break;
                }
                HANDLE input = CreateFileA(archive, GENERIC_READ,
                                           FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                           FILE_FLAG_OPEN_REPARSE_POINT, NULL);
                if (input == INVALID_HANDLE_VALUE) {
                    /* The legacy archive changed after the scan. Retry update.
                     */
                    ok = 0;
                    break;
                }
                wpm_package_info fresh;
                BY_HANDLE_FILE_INFORMATION attributes;
                ok = GetFileInformationByHandle(input, &attributes) &&
                     !(attributes.dwFileAttributes &
                       (FILE_ATTRIBUTE_DIRECTORY |
                        FILE_ATTRIBUTE_REPARSE_POINT)) &&
                     wpm_archive_inspect_open(input, &fresh);
                if (ok) {
                    strcpy_s(fresh.archive_name, sizeof(fresh.archive_name),
                             items[i].archive_name);
                    ok = save_record(&fresh, 0);
                }
                CloseHandle(input);
                if (!ok) {
                    break;
                }
            }
        }
    }
    free(items);
    if (!ok) {
        wpm_event_printf(
            WPM_EVENT_ERROR, "installed.migration.failed", "installed",
            "migrate",
            "Error: could not migrate legacy installation records.\n");
    }
    return ok;
}

int wpm_installed_forget(const char *archive_name) {
    char canonical[RECORD_PATH], path[RECORD_PATH], name[80];
    wpm_path_guard *guard = NULL;
    int ok = 0;
    if (!record_name(archive_name, name) ||
        !wpm_require_capability(WPM_CAP_WRITE, "remove installed record")) {
        return 0;
    }
    wpm_path_result state = record_root(0, canonical, &guard);
    if (state == WPM_PATH_ABSENT) {
        ok = 1;
    } else if (state == WPM_PATH_OPEN &&
               snprintf(path, sizeof(path), "%s\\%s", canonical, name) <
                   sizeof(path)) {
        ok = DeleteFileA(path) || GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    wpm_path_guard_close(guard);
    return ok;
}
