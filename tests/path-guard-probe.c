/** @file path-guard-probe.c @brief Real directory substitution boundary tests.
 */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <windows.h>
#include "path_guard.h"
#include "logging.h"

int wpm_event_printf(wpm_event_class kind, const char *code,
                     const char *operation, const char *phase,
                     const char *format, ...) {
    (void)kind;
    (void)code;
    (void)operation;
    (void)phase;
    (void)format;
    return 0;
}

/* Mount-point reparse points require no symbolic-link privilege. The fixture
 * uses only freshly created directories, and never traverses the junction. */
static int set_junction(HANDLE directory, const char *target) {
    struct {
        DWORD tag;
        WORD length, reserved;
        WORD substitute_offset, substitute_length;
        WORD print_offset, print_length;
        WCHAR path[1024];
    } data;
    char substitute[MAX_PATH + 8];
    int count;
    DWORD returned;
    memset(&data, 0, sizeof(data));
    snprintf(substitute, sizeof(substitute), "\\??\\%s", target);
    count = MultiByteToWideChar(CP_ACP, 0, substitute, -1, data.path, 1024);
    if (!count) {
        return 0;
    }
    data.tag = 0xa0000003UL;
    data.substitute_length = (WORD)((count - 1) * sizeof(WCHAR));
    data.print_offset = (WORD)(count * sizeof(WCHAR));
    data.length = (WORD)(8 + (count + 1) * sizeof(WCHAR));
    return DeviceIoControl(directory, 0x000900a4UL, &data, 8 + data.length,
                           NULL, 0, &returned, NULL) != 0;
}

int main(int argc, char **argv) {
    char root[MAX_PATH], target[MAX_PATH], child[MAX_PATH], moved[MAX_PATH];
    char canonical[MAX_PATH];
    wpm_path_guard *guard = NULL;
    wpm_path_guard *nested = NULL;
    HANDLE writer = INVALID_HANDLE_VALUE, file = INVALID_HANDLE_VALUE;
    int success = 0;
    if (argc != 2 || !GetFullPathNameA(argv[1], sizeof(root), root, NULL) ||
        strlen(root) + 16 >= sizeof(root)) {
        return 1;
    }
    snprintf(target, sizeof(target), "%s\\target", root);
    snprintf(child, sizeof(child), "%s\\guarded", root);
    snprintf(moved, sizeof(moved), "%s\\moved", root);
    /* The caller supplies an absent fixture root; existing state is untouched.
     */
    if (!CreateDirectoryA(root, NULL)) {
        return 1;
    }
    if (!CreateDirectoryA(target, NULL) || !CreateDirectoryA(child, NULL)) {
        goto done;
    }
    if (wpm_path_guard_open(child, 0, canonical, sizeof(canonical), &guard) !=
        WPM_PATH_OPEN) {
        goto done;
    }
    if (MoveFileA(child, moved) || RemoveDirectoryA(child)) {
        goto done;
    }
    snprintf(canonical, sizeof(canonical), "%s\\ordinary.tmp", child);
    file = wpm_path_guard_file(guard, "ordinary.tmp", 1);
    if (!file) {
        file = INVALID_HANDLE_VALUE;
        goto done;
    }
    if (!wpm_path_guard_publish(guard, file, "published.tmp", 0)) {
        goto done;
    }
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    snprintf(canonical, sizeof(canonical), "%s\\published.tmp", child);
    file = wpm_path_guard_file(guard, "published.tmp", 1);
    if (file) {
        goto done;
    }
    file = wpm_path_guard_file(guard, "published.tmp", 0);
    if (!file || wpm_path_guard_publish(guard, file, "moved.tmp", 0) ||
        wpm_path_guard_discard_fresh(guard, file)) {
        goto done;
    }
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    if (!DeleteFileA(canonical)) {
        goto done;
    }
    file = wpm_path_guard_file(guard, "discard.tmp", 1);
    if (!file || !wpm_path_guard_discard_fresh(guard, file) ||
        wpm_path_guard_discard_fresh(guard, file) ||
        wpm_path_guard_publish(guard, file, "discard-moved.tmp", 0)) {
        goto done;
    }
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    snprintf(canonical, sizeof(canonical), "%s\\discard.tmp", child);
    if (GetFileAttributesA(canonical) != INVALID_FILE_ATTRIBUTES ||
        GetLastError() != ERROR_FILE_NOT_FOUND) {
        goto done;
    }
    file = wpm_path_guard_file(guard, "older.tmp", 1);
    if (!file) {
        goto done;
    }
    writer = wpm_path_guard_file(guard, "newer.tmp", 1);
    if (!writer || wpm_path_guard_discard_fresh(guard, file) ||
        !wpm_path_guard_discard_fresh(guard, writer)) {
        goto done;
    }
    CloseHandle(writer);
    writer = INVALID_HANDLE_VALUE;
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    snprintf(canonical, sizeof(canonical), "%s\\older.tmp", child);
    if (GetFileAttributesA(canonical) == INVALID_FILE_ATTRIBUTES ||
        !DeleteFileA(canonical)) {
        goto done;
    }
    nested = wpm_path_guard_child(guard, ".wpm", 1);
    if (!nested) {
        goto done;
    }
    {
        wpm_path_guard *unknown = wpm_path_guard_child(guard, ".wpm", 1);
        if (unknown) {
            wpm_path_guard_close(unknown);
            goto done;
        }
    }
    file = wpm_path_guard_file(nested, ".hidden", 1);
    if (!file) {
        goto done;
    }
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    wpm_path_guard_close(guard);
    guard = NULL;
    /* The child owns its ancestor handles, independently of the parent. */
    file = wpm_path_guard_file(nested, ".hidden", 0);
    if (!file) {
        goto done;
    }
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    wpm_path_guard_close(nested);
    nested = NULL;
    snprintf(canonical, sizeof(canonical), "%s\\.wpm\\.hidden", child);
    if (!DeleteFileA(canonical)) {
        goto done;
    }
    snprintf(canonical, sizeof(canonical), "%s\\.wpm", child);
    if (!RemoveDirectoryA(canonical) ||
        wpm_path_guard_open(child, 0, canonical, sizeof(canonical), &guard) !=
            WPM_PATH_OPEN) {
        goto done;
    }
    snprintf(canonical, sizeof(canonical), "%s\\outside.tmp", target);
    file = CreateFileA(canonical, GENERIC_READ | GENERIC_WRITE | DELETE, 0,
                       NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE ||
        wpm_path_guard_publish(guard, file, "unowned.tmp", 0) ||
        wpm_path_guard_discard_fresh(guard, file)) {
        goto done;
    }
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    if (!DeleteFileA(canonical)) {
        goto done;
    }
    writer = CreateFileA(
        child, FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (writer == INVALID_HANDLE_VALUE || !set_junction(writer, target)) {
        puts("FAIL: fixture could not demonstrate real reparse conversion");
        goto done;
    }
    CloseHandle(writer);
    writer = INVALID_HANDLE_VALUE;
    /* Sharing alone cannot prevent metadata-handle junction conversion.
     * Guarded I/O must detect it and leave the outside target untouched. */
    file = wpm_path_guard_file(guard, "outside.tmp", 1);
    if (file) {
        goto done;
    }
    file = INVALID_HANDLE_VALUE;
    {
        char name[256];
        if (wpm_path_guard_next_json(guard, name, sizeof(name), 1) != -1) {
            goto done;
        }
    }
    snprintf(canonical, sizeof(canonical), "%s\\outside.tmp", target);
    if (GetFileAttributesA(canonical) != INVALID_FILE_ATTRIBUTES) {
        goto done;
    }
    wpm_path_guard_close(guard);
    guard = NULL;
    if (wpm_path_guard_open(child, 0, canonical, sizeof(canonical), &guard) !=
        WPM_PATH_UNSAFE) {
        goto done;
    }
    wpm_path_guard_close(guard);
    guard = NULL;
    if (!RemoveDirectoryA(child) || !CreateDirectoryA(child, NULL)) {
        goto done;
    }
    if (wpm_path_guard_open(child, 2, canonical, sizeof(canonical), &guard) !=
        WPM_PATH_UNSAFE) {
        goto done;
    }
    success = 1;
done:
    wpm_path_guard_close(nested);
    wpm_path_guard_close(guard);
    if (writer != INVALID_HANDLE_VALUE) {
        CloseHandle(writer);
    }
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    /* No recursive deletion: remove only fixture names, including junction. */
    RemoveDirectoryA(child);
    RemoveDirectoryA(target);
    RemoveDirectoryA(root);
    if (success) {
        puts("PASS: guarded I/O rejects reparse conversion; child "
             "creation/publication allowed");
    }
    return success ? 0 : 1;
}
