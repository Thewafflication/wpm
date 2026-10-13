/** @file path_guard.c @brief Windows directory identity boundary.
 * At most 128 ancestors are held without delete sharing. Opening checks each
 * component through its own handle before proceeding beneath it. Missing
 * directories are created only in the explicit write-capable bootstrap mode.
 * Failure retains a releasable partial guard and never follows a reparse root.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "path_guard.h"
#include "capability.h"

struct wpm_path_guard {
    HANDLE handles[128];
    size_t count;
    DWORD created_identity[3];
    int created_known;
};

/* XP-compatible native ABI, with named C99 union and checked pointer-width
 * layout. See FILE_RENAME_INFORMATION and IO_STATUS_BLOCK in the Windows SDK.
 */
typedef struct {
    union {
        LONG status;
        void *pointer;
    } value;
    ULONG_PTR information;
} guard_io_status;
typedef struct {
    BYTE replace;
    HANDLE root;
    ULONG length;
    WCHAR name[1];
} guard_rename_info;
typedef char
    guard_io_layout[sizeof(guard_io_status) == 2 * sizeof(void *) ? 1 : -1];
typedef char
    guard_rename_layout[offsetof(guard_rename_info, root) == sizeof(void *) &&
                                offsetof(guard_rename_info, name) ==
                                    2 * sizeof(void *) + sizeof(ULONG)
                            ? 1
                            : -1];

typedef struct {
    USHORT length, capacity;
    WCHAR *buffer;
} guard_unicode;
typedef struct {
    ULONG length;
    HANDLE root;
    guard_unicode *name;
    ULONG attributes;
    void *security, *quality;
} guard_attributes;
typedef char guard_attributes_layout
    [sizeof(guard_attributes) == (sizeof(void *) == 8 ? 48 : 24) ? 1 : -1];

static HANDLE guard_open_relative(HANDLE parent, const char *name, DWORD access,
                                  DWORD sharing, ULONG disposition,
                                  int directory) {
    typedef LONG(WINAPI * create_file)(
        HANDLE *, DWORD, guard_attributes *, guard_io_status *, LARGE_INTEGER *,
        ULONG, ULONG, ULONG, ULONG, void *, ULONG);
    typedef ULONG(WINAPI * dos_error)(LONG);
    create_file create;
    dos_error error;
    WCHAR wide[256];
    guard_unicode unicode;
    guard_attributes attributes;
    guard_io_status result;
    HANDLE file = INVALID_HANDLE_VALUE;
    LONG status;
    int length = MultiByteToWideChar(CP_ACP, 0, name, -1, wide, 256);
    if (!length || length < 2) {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    create = (create_file)GetProcAddress(GetModuleHandleA("ntdll.dll"),
                                         "NtCreateFile");
    error = (dos_error)GetProcAddress(GetModuleHandleA("ntdll.dll"),
                                      "RtlNtStatusToDosError");
    if (!create || !error) {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return INVALID_HANDLE_VALUE;
    }
    unicode.length = (USHORT)((length - 1) * sizeof(WCHAR));
    unicode.capacity = (USHORT)(length * sizeof(WCHAR));
    unicode.buffer = wide;
    memset(&attributes, 0, sizeof(attributes));
    attributes.length = sizeof(attributes);
    attributes.root = parent;
    attributes.name = &unicode;
    attributes.attributes = 0x40; /* OBJ_CASE_INSENSITIVE */
    memset(&result, 0, sizeof(result));
    status = create(&file, access | SYNCHRONIZE, &attributes, &result, NULL,
                    FILE_ATTRIBUTE_NORMAL, sharing, disposition,
                    0x00200020UL | (directory ? 1 : 0x40), NULL, 0);
    if (status != 0) {
        if (file != INVALID_HANDLE_VALUE && file) {
            CloseHandle(file);
        }
        SetLastError(error(status));
        return INVALID_HANDLE_VALUE;
    }
    return file;
}

static int guard_ordinary(wpm_path_guard *guard) {
    BY_HANDLE_FILE_INFORMATION info;
    if (!guard || !guard->count) {
        return 0;
    }
    for (size_t i = 0; i < guard->count; i++) {
        if (!GetFileInformationByHandle(guard->handles[i], &info) ||
            !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            SetLastError(ERROR_ACCESS_DENIED);
            return 0;
        }
    }
    return 1;
}

void *wpm_path_guard_file(wpm_path_guard *guard, const char *name, int fresh) {
    HANDLE file;
    BY_HANDLE_FILE_INFORMATION info;
    size_t length;
    if ((fresh != 0 && fresh != 1) || !name || !(length = strlen(name)) ||
        length > 255 || name[0] == '.' || name[length - 1] == '.' ||
        name[length - 1] == ' ' || strchr(name, '\\') || strchr(name, '/') ||
        strchr(name, ':') ||
        (fresh &&
         !wpm_require_capability(WPM_CAP_WRITE, "guarded file creation")) ||
        !guard_ordinary(guard)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return NULL;
    }
    for (size_t i = 0; i < length; i++) {
        if ((unsigned char)name[i] < 32 || (unsigned char)name[i] == 127) {
            SetLastError(ERROR_INVALID_NAME);
            return NULL;
        }
    }
    file = guard_open_relative(guard->handles[guard->count - 1], name,
                               fresh ? GENERIC_READ | GENERIC_WRITE | DELETE
                                     : GENERIC_READ,
                               fresh ? 0 : FILE_SHARE_READ, fresh ? 2 : 1, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    if (!guard_ordinary(guard) || GetFileType(file) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(file, &info) || info.nNumberOfLinks != 1 ||
        (info.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(file);
        SetLastError(ERROR_ACCESS_DENIED);
        return NULL;
    }
    if (fresh) {
        guard->created_identity[0] = info.dwVolumeSerialNumber;
        guard->created_identity[1] = info.nFileIndexHigh;
        guard->created_identity[2] = info.nFileIndexLow;
        guard->created_known = 1;
    }
    return file;
}

int wpm_path_guard_next_json(wpm_path_guard *guard, char *name, size_t size,
                             int restart) {
    typedef LONG(WINAPI * query_directory)(HANDLE, HANDLE, void *, void *,
                                           guard_io_status *, void *, ULONG,
                                           int, BYTE, guard_unicode *, BYTE);
    struct {
        ULONG next, index, length;
        WCHAR name[256];
    } entry;
    WCHAR mask[] = L"*.json";
    guard_unicode pattern;
    guard_io_status result;
    query_directory query;
    LONG status;
    int count;
    if (!name || size < 2 || size > 256 || !guard_ordinary(guard)) {
        return -1;
    }
    query = (query_directory)GetProcAddress(GetModuleHandleA("ntdll.dll"),
                                            "NtQueryDirectoryFile");
    if (!query) {
        return -1;
    }
    pattern.length = sizeof(mask) - sizeof(WCHAR);
    pattern.capacity = sizeof(mask);
    pattern.buffer = mask;
    memset(&result, 0, sizeof(result));
    memset(&entry, 0, sizeof(entry));
    status = query(guard->handles[guard->count - 1], NULL, NULL, NULL, &result,
                   &entry, sizeof(entry), 12, 1, restart ? &pattern : NULL,
                   restart != 0);
    if (!guard_ordinary(guard)) {
        return -1;
    }
    if ((ULONG)status == 0x80000006UL || (ULONG)status == 0xc000000fUL) {
        return 0;
    }
    if (status != 0 || result.information < 12 ||
        result.information > sizeof(entry) || entry.length == 0 ||
        entry.length > sizeof(entry.name) || entry.length % sizeof(WCHAR) ||
        12 + entry.length > result.information) {
        return -1;
    }
    count =
        WideCharToMultiByte(CP_ACP, 0, entry.name, entry.length / sizeof(WCHAR),
                            name, (int)size - 1, NULL, NULL);
    if (!count) {
        return -1;
    }
    name[count] = 0;
    return 1;
}

wpm_path_result wpm_path_guard_open(const char *path, int create,
                                    char *canonical, size_t size,
                                    wpm_path_guard **result) {
    wpm_path_guard *guard;
    DWORD length;
    if (!result) {
        return WPM_PATH_FAILED;
    }
    *result = NULL;
    if (!path || !canonical || size > MAXDWORD || create < 0 || create > 2 ||
        (create &&
         !wpm_require_capability(WPM_CAP_WRITE, "directory bootstrap"))) {
        return WPM_PATH_FAILED;
    }
    length = GetFullPathNameA(path, (DWORD)size, canonical, NULL);
    if (!length || length >= size) {
        return WPM_PATH_FAILED;
    }
    while (length > 3 && canonical[length - 1] == '\\') {
        canonical[--length] = 0;
    }
    if (length < 3 || !isalpha((unsigned char)canonical[0]) ||
        canonical[1] != ':' || canonical[2] != '\\') {
        return WPM_PATH_UNSAFE;
    }
    if (create == 2 && length == 3) {
        return WPM_PATH_UNSAFE;
    }
    for (size_t i = 3; i <= length; i++) {
        unsigned char value = (unsigned char)canonical[i];
        if (value == ':' || (value && (value < 32 || value == 127)) ||
            ((i == length || value == '\\') &&
             (canonical[i - 1] == '.' || canonical[i - 1] == ' '))) {
            return WPM_PATH_UNSAFE;
        }
    }
    guard = calloc(1, sizeof(*guard));
    if (!guard) {
        return WPM_PATH_FAILED;
    }
    *result = guard;
    for (size_t i = 3; i <= length; i++) {
        char saved;
        HANDLE directory;
        BY_HANDLE_FILE_INFORMATION info;
        DWORD error;
        if (i != 3 && i != length && canonical[i] != '\\') {
            continue;
        }
        if (guard->count == 128) {
            return WPM_PATH_UNSAFE;
        }
        saved = canonical[i];
        canonical[i] = 0;
        if (i == 3) {
            directory = CreateFileA(
                canonical,
                FILE_READ_ATTRIBUTES | FILE_TRAVERSE | FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                NULL);
        } else {
            const char *component = strrchr(canonical, '\\') + 1;
            if (!guard_ordinary(guard)) {
                canonical[i] = saved;
                return WPM_PATH_UNSAFE;
            }
            directory = guard_open_relative(
                guard->handles[guard->count - 1], component,
                FILE_READ_ATTRIBUTES | FILE_TRAVERSE | FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                create == 2 && i == length ? 2
                : create                   ? 3
                                           : 1,
                1);
        }
        error = directory == INVALID_HANDLE_VALUE ? GetLastError() : 0;
        canonical[i] = saved;
        if (directory == INVALID_HANDLE_VALUE) {
            if (error == ERROR_FILE_NOT_FOUND ||
                error == ERROR_PATH_NOT_FOUND) {
                return WPM_PATH_ABSENT;
            }
            if (create == 2 && i == length && error == ERROR_ALREADY_EXISTS) {
                return WPM_PATH_UNSAFE;
            }
            return WPM_PATH_FAILED;
        }
        guard->handles[guard->count++] = directory;
        if (!GetFileInformationByHandle(directory, &info) ||
            !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            return WPM_PATH_UNSAFE;
        }
    }
    return WPM_PATH_OPEN;
}

int wpm_path_guard_publish(wpm_path_guard *guard, void *file, const char *name,
                           int replace) {
    typedef LONG(WINAPI * set_information)(HANDLE, guard_io_status *, void *,
                                           ULONG, int);
    set_information set;
    guard_rename_info *info;
    guard_io_status result;
    size_t length, size;
    LONG status;
    BY_HANDLE_FILE_INFORMATION identity;
    if (!guard || !guard->count || !file || file == INVALID_HANDLE_VALUE ||
        !name || !(length = strlen(name)) || length > 255 ||
        !guard_ordinary(guard) ||
        !wpm_require_capability(WPM_CAP_WRITE, "recovery publication")) {
        return 0;
    }
    if (!guard->created_known || !GetFileInformationByHandle(file, &identity) ||
        identity.nNumberOfLinks != 1 ||
        (identity.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT |
          FILE_ATTRIBUTE_READONLY)) ||
        identity.dwVolumeSerialNumber != guard->created_identity[0] ||
        identity.nFileIndexHigh != guard->created_identity[1] ||
        identity.nFileIndexLow != guard->created_identity[2]) {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '.') {
            return 0;
        }
    }
    if (name[0] == '.' || name[length - 1] == '.') {
        return 0;
    }
    set = (set_information)GetProcAddress(GetModuleHandleA("ntdll.dll"),
                                          "NtSetInformationFile");
    if (!set) {
        return 0;
    }
    size = sizeof(*info) + length * sizeof(WCHAR);
    info = calloc(1, size);
    if (!info) {
        return 0;
    }
    info->replace = replace != 0;
    /* A simple name with a NULL root renames in the source file's own parent.
     * No close/reopen of the exclusive source or pathname traversal occurs. */
    info->length = (ULONG)(length * sizeof(WCHAR));
    for (size_t i = 0; i < length; i++) {
        info->name[i] = (WCHAR)(unsigned char)name[i];
    }
    memset(&result, 0, sizeof(result));
    status = set((HANDLE)file, &result, info, (ULONG)size, 10);
    free(info);
    return status == 0;
}

void wpm_path_guard_close(wpm_path_guard *guard) {
    if (guard) {
        while (guard->count) {
            CloseHandle(guard->handles[--guard->count]);
        }
        free(guard);
    }
}
