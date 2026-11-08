/** @file file_processes.c @brief Discover file users on Windows NT 5.0 and
 * later.
 */
#include "file_processes.h"
#include "capability.h"
#include "logging.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* Local ABI declarations keep the build independent of newer SDK headers and
 * avoid importing Vista APIs into the Windows 2000/XP executable. */
typedef struct {
    DWORD pid;
    FILETIME started;
} file_user_identity;
typedef struct {
    file_user_identity process;
    WCHAR application[256];
    WCHAR service[64];
    int type;
    ULONG status;
    DWORD session;
    BOOL restartable;
} file_user;
typedef DWORD(WINAPI *rm_start)(DWORD *, DWORD, WCHAR *);
typedef DWORD(WINAPI *rm_register)(DWORD, UINT, LPCWSTR *, UINT,
                                   file_user_identity *, UINT, LPCWSTR *);
typedef DWORD(WINAPI *rm_list)(DWORD, UINT *, UINT *, file_user *, DWORD *);
typedef DWORD(WINAPI *rm_end)(DWORD);
typedef struct {
    DWORD dwSize, cntUsage, th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD th32ModuleID, cntThreads, th32ParentProcessID;
    LONG pcPriClassBase;
    DWORD dwFlags;
    CHAR szExeFile[MAX_PATH];
} process_entry;
typedef HANDLE(WINAPI *snapshot_fn)(DWORD, DWORD);
typedef BOOL(WINAPI *process_entry_fn)(HANDLE, process_entry *);

typedef LONG(WINAPI *query_process_fn)(HANDLE, ULONG, void *, ULONG, ULONG *);

static int failure(const char *operation, DWORD error) {
    wpm_event_printf(WPM_EVENT_ERROR, "file-processes.failed", "file-processes",
                     operation, "Error: %s failed (Windows error %lu).\n",
                     operation, (unsigned long)error);
    return 1;
}

static int installer_ancestor(DWORD pid) {
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    snapshot_fn create_snapshot =
        (snapshot_fn)GetProcAddress(kernel32, "CreateToolhelp32Snapshot");
    process_entry_fn first =
        (process_entry_fn)GetProcAddress(kernel32, "Process32First");
    process_entry_fn next =
        (process_entry_fn)GetProcAddress(kernel32, "Process32Next");
    HANDLE snapshot;
    DWORD current = GetCurrentProcessId();
    process_entry entry;
    int protected = 1;
    if (!create_snapshot || !first || !next) {
        return 1;
    }
    snapshot = create_snapshot(0x00000002, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 1;
    }
    for (int depth = 0; depth < 64; depth++) {
        DWORD parent = 0;
        if (current == pid) {
            break;
        }
        entry.dwSize = sizeof(entry);
        if (!first(snapshot, &entry)) {
            break;
        }
        do {
            if (entry.th32ProcessID == current) {
                parent = entry.th32ParentProcessID;
                break;
            }
        } while (next(snapshot, &entry));
        if (!parent) {
            protected = 0;
            break;
        }
        if (parent == current) {
            break;
        }
        current = parent;
    }
    CloseHandle(snapshot);
    return protected;
}

static int stop_user(const file_user *user) {
    HANDLE process;
    FILETIME created, exited, kernel, cpu;
    DWORD error;
    /* Services, Explorer, critical processes and this invocation require their
     * own lifecycle management; do not force-kill them through a file helper.
     */
    if (user->type == 3 || user->type == 4 || user->type == 1000 ||
        user->process.pid <= 4 || installer_ancestor(user->process.pid)) {
        return failure("refusing protected/service process",
                       ERROR_ACCESS_DENIED);
    }
    if (!wpm_require_capability(WPM_CAP_WRITE | WPM_CAP_SCRIPT,
                                "terminate a file-using process")) {
        return 1;
    }
    process =
        OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                    FALSE, user->process.pid);
    if (!process) {
        error = GetLastError();
        return error == ERROR_INVALID_PARAMETER ? 0
                                                : failure("OpenProcess", error);
    }
    if (!GetProcessTimes(process, &created, &exited, &kernel, &cpu)) {
        error = GetLastError();
        CloseHandle(process);
        return failure("GetProcessTimes", error);
    }
    /* Check creation time on the same handle used to terminate: a recycled PID
     * must never target an unrelated process. */
    if (CompareFileTime(&created, &user->process.started) != 0 ||
        WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
        CloseHandle(process);
        return 0;
    }
    {
        query_process_fn query = (query_process_fn)GetProcAddress(
            GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
        ULONG critical = 0;
        /* Check the same process handle used to terminate. NT 5.0 may reject
         * this class; its system images are protected by the legacy scanner. */
        if (query &&
            query(process, 29, &critical, sizeof(critical), NULL) >= 0 &&
            critical) {
            CloseHandle(process);
            return failure("refusing critical process", ERROR_ACCESS_DENIED);
        }
    }
    if (!TerminateProcess(process, 1)) {
        error = GetLastError();
        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
            CloseHandle(process);
            return 0;
        }
        CloseHandle(process);
        return failure("TerminateProcess", error);
    }
    error = WaitForSingleObject(process, 10000);
    CloseHandle(process);
    return error == WAIT_OBJECT_0 ? 0 : failure("wait for process exit", error);
}

/* Legacy NT handle-table ABI. Class 64 is preferred; NT 5.0 falls back to 16.
 * No object-name queries are made: those can hang on pipe handles. */
typedef LONG(WINAPI *query_system_fn)(ULONG, void *, ULONG, ULONG *);
typedef struct {
    void *object;
    ULONG_PTR pid, handle;
    ULONG access;
    USHORT trace, type;
    ULONG attributes, reserved;
} extended_handle;
typedef struct {
    ULONG_PTR count, reserved;
    extended_handle entries[1];
} extended_handles;
typedef struct {
    USHORT pid, trace;
    BYTE type, attributes;
    USHORT handle;
    void *object;
    ULONG access;
} old_handle;
typedef struct {
    ULONG count;
    old_handle entries[1];
} old_handles;
typedef struct {
    DWORD size, module_id, pid, global_usage, process_usage;
    BYTE *base;
    DWORD bytes;
    HMODULE module;
    WCHAR name[256], path[MAX_PATH];
} module_entry;
typedef BOOL(WINAPI *module_entry_fn)(HANDLE, module_entry *);
typedef struct {
    HANDLE handle;
    BY_HANDLE_FILE_INFORMATION info;
} legacy_target;
typedef struct {
    char *name, *display;
    DWORD type, state, controls, win32_exit, service_exit;
    DWORD checkpoint, hint, pid, flags;
} legacy_service;
typedef HANDLE(WINAPI *open_scm_fn)(const char *, const char *, DWORD);
typedef BOOL(WINAPI *enum_services_fn)(HANDLE, int, DWORD, DWORD, BYTE *, DWORD,
                                       DWORD *, DWORD *, DWORD *, const char *);
typedef BOOL(WINAPI *close_scm_fn)(HANDLE);

static int same_file(const BY_HANDLE_FILE_INFORMATION *a,
                     const BY_HANDLE_FILE_INFORMATION *b) {
    return a->dwVolumeSerialNumber == b->dwVolumeSerialNumber &&
           a->nFileIndexHigh == b->nFileIndexHigh &&
           a->nFileIndexLow == b->nFileIndexLow;
}

static int legacy_matches(HANDLE handle, legacy_target *targets, UINT count) {
    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileType(handle) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(handle, &info)) {
        return 0;
    }
    for (UINT i = 0; i < count; i++) {
        if (same_file(&info, &targets[i].info)) {
            return 1;
        }
    }
    return 0;
}

static int legacy_add(file_user **users, UINT *count, HANDLE process, DWORD pid,
                      const char *name) {
    file_user *grown;
    FILETIME created, exited, kernel, cpu;
    for (UINT i = 0; i < *count; i++) {
        if ((*users)[i].process.pid == pid) {
            return 1;
        }
    }
    if (*count >= 65536 ||
        !GetProcessTimes(process, &created, &exited, &kernel, &cpu)) {
        return 0;
    }
    grown = realloc(*users, ((size_t)*count + 1) * sizeof(**users));
    if (!grown) {
        return 0;
    }
    *users = grown;
    memset(&grown[*count], 0, sizeof(*grown));
    grown[*count].process.pid = pid;
    grown[*count].process.started = created;
    MultiByteToWideChar(CP_ACP, 0, name, -1, grown[*count].application, 256);
    (*count)++;
    return 1;
}

/* Service PIDs are obtained through an NT 5.0 API, not guessed from names.
 * If the SCM cannot be inspected, kill mode must fail closed. */
static int legacy_guard(file_user *users, UINT count) {
    WCHAR path[MAX_PATH];
    HMODULE library;
    open_scm_fn open_scm;
    enum_services_fn enumerate;
    close_scm_fn close_scm;
    HANDLE scm;
    BYTE *buffer = NULL;
    DWORD needed = 0, returned = 0, resume = 0, size = 32768;
    int result = 0;
    if (!GetSystemDirectoryW(path, MAX_PATH) || wcslen(path) > MAX_PATH - 14) {
        return 0;
    }
    wcscat(path, L"\\advapi32.dll");
    library = LoadLibraryW(path);
    if (!library) {
        return 0;
    }
    open_scm = (open_scm_fn)GetProcAddress(library, "OpenSCManagerA");
    enumerate =
        (enum_services_fn)GetProcAddress(library, "EnumServicesStatusExA");
    close_scm = (close_scm_fn)GetProcAddress(library, "CloseServiceHandle");
    if (!open_scm || !enumerate || !close_scm) {
        goto done;
    }
    scm = open_scm(NULL, NULL, 4 /* SC_MANAGER_ENUMERATE_SERVICE */);
    if (!scm) {
        goto done;
    }
    buffer = malloc(size);
    if (buffer) {
        for (int attempt = 0; attempt < 128; attempt++) {
            BOOL success = enumerate(scm, 0, 0x30, 3, buffer, size, &needed,
                                     &returned, &resume, NULL);
            DWORD error = success ? 0 : GetLastError();
            legacy_service *services = (legacy_service *)buffer;
            for (DWORD i = 0; i < returned; i++) {
                for (UINT j = 0; j < count; j++) {
                    if (services[i].pid == users[j].process.pid) {
                        users[j].type = 3;
                    }
                }
            }
            if (success) {
                result = 1;
                break;
            }
            if (error != ERROR_MORE_DATA) {
                break;
            }
            if (!returned) {
                /* Windows 2000 limits this buffer to 64 KiB. */
                if (needed > 65536 || size == 65536) {
                    break;
                }
                free(buffer);
                size = 65536;
                buffer = malloc(size);
                if (!buffer) {
                    break;
                }
            }
        }
    }
    close_scm(scm);
done:
    free(buffer);
    FreeLibrary(library);
    return result;
}

static int legacy_critical(const char *name) {
    static const char *names[] = {
        "System",      "System Idle Process", "smss.exe",     "csrss.exe",
        "wininit.exe", "winlogon.exe",        "services.exe", "lsass.exe",
        "lsm.exe",     "svchost.exe",         "explorer.exe"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (!_stricmp(name, names[i])) {
            return 1;
        }
    }
    return 0;
}

static DWORD legacy_list(WCHAR **paths, UINT path_count, file_user **users,
                         UINT *count) {
    HMODULE kernel = GetModuleHandleA("kernel32.dll");
    query_system_fn query = (query_system_fn)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    snapshot_fn snapshot =
        (snapshot_fn)GetProcAddress(kernel, "CreateToolhelp32Snapshot");
    process_entry_fn first =
        (process_entry_fn)GetProcAddress(kernel, "Process32First");
    process_entry_fn next =
        (process_entry_fn)GetProcAddress(kernel, "Process32Next");
    module_entry_fn module_first =
        (module_entry_fn)GetProcAddress(kernel, "Module32FirstW");
    module_entry_fn module_next =
        (module_entry_fn)GetProcAddress(kernel, "Module32NextW");
    legacy_target *targets = calloc(path_count, sizeof(*targets));
    void *table = NULL;
    HANDLE processes = INVALID_HANDLE_VALUE;
    ULONG bytes = 65536, required = 0;
    ULONG_PTR handle_count = 0;
    USHORT file_type = 0;
    int extended = 1;
    LONG status;
    DWORD error = ERROR_OUTOFMEMORY, skipped = 0;
    process_entry entry;
    if (!targets) {
        return error;
    }
    if (!query || !snapshot || !first || !next || !module_first ||
        !module_next) {
        error = ERROR_PROC_NOT_FOUND;
        goto done;
    }
    for (UINT i = 0; i < path_count; i++) {
        targets[i].handle =
            CreateFileW(paths[i], 0, 7, NULL, OPEN_EXISTING, 0, NULL);
        if (targets[i].handle == INVALID_HANDLE_VALUE ||
            !GetFileInformationByHandle(targets[i].handle, &targets[i].info)) {
            error = GetLastError();
            goto done;
        }
        if (!targets[i].info.nFileIndexHigh && !targets[i].info.nFileIndexLow) {
            error = ERROR_NOT_SUPPORTED;
            goto done;
        }
    }
    for (;;) {
        void *grown = realloc(table, bytes);
        if (!grown) {
            error = ERROR_OUTOFMEMORY;
            goto done;
        }
        table = grown;
        status = query(extended ? 64 : 16, table, bytes, &required);
        if (extended &&
            (status == (LONG)0xc0000003 || status == (LONG)0xc0000002)) {
            extended = 0;
            continue;
        }
        if (status >= 0) {
            break;
        }
        if (status != (LONG)0xc0000004 || bytes >= 64 * 1024 * 1024) {
            error = ERROR_NOT_SUPPORTED;
            goto done;
        }
        bytes *= 2;
    }
    handle_count = extended ? ((extended_handles *)table)->count
                            : ((old_handles *)table)->count;
    /* Validate the native count before walking an internal OS ABI buffer. */
    if (handle_count >
        (bytes -
         (extended ? sizeof(ULONG_PTR) * 2 : offsetof(old_handles, entries))) /
            (extended ? sizeof(extended_handle) : sizeof(old_handle))) {
        error = ERROR_INVALID_DATA;
        goto done;
    }
    for (ULONG_PTR i = 0; i < handle_count; i++) {
        ULONG_PTR pid = extended ? ((extended_handles *)table)->entries[i].pid
                                 : ((old_handles *)table)->entries[i].pid;
        ULONG_PTR value = extended
                              ? ((extended_handles *)table)->entries[i].handle
                              : ((old_handles *)table)->entries[i].handle;
        if (pid == GetCurrentProcessId() &&
            value == (ULONG_PTR)targets[0].handle) {
            file_type = extended ? ((extended_handles *)table)->entries[i].type
                                 : ((old_handles *)table)->entries[i].type;
            break;
        }
    }
    if (!file_type) {
        error = ERROR_INVALID_DATA;
        goto done;
    }
    processes = snapshot(2, 0);
    if (processes == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        goto done;
    }
    memset(&entry, 0, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (!first(processes, &entry)) {
        error = GetLastError();
        goto done;
    }
    do {
        HANDLE process, modules;
        int matched = 0;
        if (entry.th32ProcessID <= 4) {
            continue;
        }
        if (!extended && entry.th32ProcessID > 65535) {
            skipped++;
        }
        process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE |
                                  SYNCHRONIZE,
                              FALSE, entry.th32ProcessID);
        if (!process) {
            skipped++;
            continue;
        }
        for (ULONG_PTR i = 0; i < handle_count && !matched; i++) {
            ULONG_PTR pid = extended
                                ? ((extended_handles *)table)->entries[i].pid
                                : ((old_handles *)table)->entries[i].pid;
            ULONG_PTR value =
                extended ? ((extended_handles *)table)->entries[i].handle
                         : ((old_handles *)table)->entries[i].handle;
            USHORT type = extended
                              ? ((extended_handles *)table)->entries[i].type
                              : ((old_handles *)table)->entries[i].type;
            HANDLE duplicate;
            int own_target = 0;
            if (pid == GetCurrentProcessId()) {
                for (UINT j = 0; j < path_count; j++) {
                    if (value == (ULONG_PTR)targets[j].handle) {
                        own_target = 1;
                    }
                }
            }
            if (pid != entry.th32ProcessID || type != file_type || own_target) {
                continue;
            }
            if (DuplicateHandle(process, (HANDLE)value, GetCurrentProcess(),
                                &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                matched = legacy_matches(duplicate, targets, path_count);
                CloseHandle(duplicate);
            }
        }
        modules = INVALID_HANDLE_VALUE;
        for (int attempt = 0; !matched && attempt < 4; attempt++) {
            modules =
                snapshot(sizeof(void *) == 8 ? 24 : 8, entry.th32ProcessID);
            if (modules != INVALID_HANDLE_VALUE ||
                GetLastError() != ERROR_BAD_LENGTH) {
                break;
            }
        }
        if (!matched && modules != INVALID_HANDLE_VALUE) {
            module_entry module;
            memset(&module, 0, sizeof(module));
            module.size = sizeof(module);
            if (module_first(modules, &module)) {
                do {
                    for (UINT i = 0; i < path_count && !matched; i++) {
                        matched = !lstrcmpiW(module.path, paths[i]);
                    }
                    if (!matched) {
                        HANDLE image = CreateFileW(module.path, 0, 7, NULL,
                                                   OPEN_EXISTING, 0, NULL);
                        if (image != INVALID_HANDLE_VALUE) {
                            matched =
                                legacy_matches(image, targets, path_count);
                            CloseHandle(image);
                        }
                    }
                } while (!matched && module_next(modules, &module));
            }
            CloseHandle(modules);
        } else if (!matched &&
                   WaitForSingleObject(process, 0) != WAIT_OBJECT_0) {
            skipped++;
        }
        if (matched && !legacy_add(users, count, process, entry.th32ProcessID,
                                   entry.szExeFile)) {
            CloseHandle(process);
            error = ERROR_OUTOFMEMORY;
            goto done;
        }
        if (matched && legacy_critical(entry.szExeFile)) {
            (*users)[*count - 1].type = 1000;
        }
        CloseHandle(process);
    } while (next(processes, &entry));
    error = GetLastError() == ERROR_NO_MORE_FILES ? 0 : GetLastError();
    if (skipped) {
        wpm_event_printf(
            WPM_EVENT_WARNING, "file-processes.partial", "file-processes",
            "legacy",
            "Warning: legacy scan could not fully inspect %lu processes; "
            "use an elevated native-architecture WPM and check copy errors.\n",
            (unsigned long)skipped);
    }
done:
    if (processes != INVALID_HANDLE_VALUE) {
        CloseHandle(processes);
    }
    for (UINT i = 0; i < path_count; i++) {
        if (targets[i].handle && targets[i].handle != INVALID_HANDLE_VALUE) {
            CloseHandle(targets[i].handle);
        }
    }
    free(targets);
    free(table);
    return error;
}

int wpm_file_processes(int argc, char **argv) {
    WCHAR **paths = calloc((size_t)argc + 1, sizeof(*paths));
    WCHAR key[33], dll[MAX_PATH];
    HMODULE library = NULL;
    rm_start start;
    rm_register resources;
    rm_list list;
    rm_end end = NULL;
    DWORD session = 0, error = 0, reasons = 0;
    UINT path_count = 0, count = 0, needed = 0;
    file_user *users = NULL;
    int kill = 0, active = 0, result = 1, literal = 0, help = 0;
    int legacy = 0;
    if (!paths) {
        return failure("allocate paths", ERROR_OUTOFMEMORY);
    }
    for (int i = 0; i < argc; i++) {
        WCHAR *input;
        DWORD size, attributes;
        int length;
        if (!literal && !strcmp(argv[i], "--")) {
            literal = 1;
            continue;
        }
        if (!literal && !strcmp(argv[i], "--legacy")) {
            legacy = 1;
            continue;
        }
        if (!literal && !strcmp(argv[i], "--kill")) {
            kill = 1;
            continue;
        }
        if (!literal && !strcmp(argv[i], "--dry-run")) {
            continue;
        }
        if (!literal &&
            (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))) {
            help = 1;
            continue;
        }
        if (!literal && argv[i][0] == '-') {
            failure("unknown file-processes option", ERROR_INVALID_PARAMETER);
            goto cleanup;
        }
        length = MultiByteToWideChar(CP_ACP, 0, argv[i], -1, NULL, 0);
        input = calloc((size_t)length, sizeof(WCHAR));
        if (!input || !length) {
            free(input);
            failure("convert file path", ERROR_OUTOFMEMORY);
            goto cleanup;
        }
        MultiByteToWideChar(CP_ACP, 0, argv[i], -1, input, length);
        if (!input[0] || wcspbrk(input, L"*?")) {
            free(input);
            failure("expected an exact file path", ERROR_INVALID_PARAMETER);
            goto cleanup;
        }
        size = GetFullPathNameW(input, 0, NULL, NULL);
        paths[path_count] = calloc((size_t)size + 1, sizeof(WCHAR));
        if (!size || !paths[path_count] ||
            !GetFullPathNameW(input, size + 1, paths[path_count], NULL)) {
            free(input);
            failure("resolve file path", ERROR_INVALID_PARAMETER);
            goto cleanup;
        }
        free(input);
        attributes = GetFileAttributesW(paths[path_count]);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            error = GetLastError();
            if (error != ERROR_FILE_NOT_FOUND &&
                error != ERROR_PATH_NOT_FOUND) {
                failure("inspect file path", error);
                goto cleanup;
            }
            /* New destination files cannot currently have file users. */
            free(paths[path_count]);
            paths[path_count] = NULL;
            continue;
        }
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) ||
            wcspbrk(paths[path_count], L"*?")) {
            failure("expected an exact file path", ERROR_INVALID_PARAMETER);
            goto cleanup;
        }
        path_count++;
    }
    if (help || !argc) {
        wpm_event_printf(
            WPM_EVENT_INFO, "file-processes.help", "file-processes", "help",
            "Usage: wpm file-processes <file...> [--kill] "
            "[--dry-run] [--legacy]\nLists PID and application for file users. "
            "--kill forcibly terminates ordinary applications.\n"
            "--legacy selects the Windows 2000/XP discovery backend.\n");
        result = help ? 0 : 1;
        goto cleanup;
    }
    /* Even an all-missing path list is valid, but an options-only call is not.
     */
    {
        int operands = 0, after_separator = 0;
        for (int i = 0; i < argc; i++) {
            if (!after_separator && !strcmp(argv[i], "--")) {
                after_separator = 1;
            } else if (after_separator || argv[i][0] != '-') {
                operands++;
            }
        }
        if (!operands) {
            failure("file path required", ERROR_INVALID_PARAMETER);
            goto cleanup;
        }
    }
    if (!path_count) {
        result = 0;
        goto cleanup;
    }
    /* Load only from the system directory, never the package staging folder. */
    if (!GetSystemDirectoryW(dll, MAX_PATH) || wcslen(dll) + 14 >= MAX_PATH) {
        failure("locate system directory", ERROR_PATH_NOT_FOUND);
        goto cleanup;
    }
    wcscat(dll, L"\\rstrtmgr.dll");
    library = legacy ? NULL : LoadLibraryW(dll);
    if (!library) {
        error = legacy_list(paths, path_count, &users, &count);
        if (error) {
            failure("legacy file-user discovery", error);
            goto cleanup;
        }
        if (kill && !wpm_is_readonly() && !legacy_guard(users, count)) {
            failure("inspect service process identities", ERROR_ACCESS_DENIED);
            goto cleanup;
        }
        legacy = 1;
        goto report;
    }
    start = (rm_start)GetProcAddress(library, "RmStartSession");
    resources = (rm_register)GetProcAddress(library, "RmRegisterResources");
    list = (rm_list)GetProcAddress(library, "RmGetList");
    end = (rm_end)GetProcAddress(library, "RmEndSession");
    if (!start || !resources || !list || !end) {
        failure("load Restart Manager entry points", ERROR_PROC_NOT_FOUND);
        goto cleanup;
    }
    error = start(&session, 0, key);
    if (error) {
        failure("RmStartSession", error);
        goto cleanup;
    }
    active = 1;
    error = resources(session, path_count, (LPCWSTR *)paths, 0, NULL, 0, NULL);
    if (error) {
        failure("RmRegisterResources", error);
        goto cleanup;
    }
    /* The list can grow between queries. Bound retries under process churn. */
    for (int attempt = 0; attempt < 8; attempt++) {
        error = list(session, &needed, &count, users, &reasons);
        if (error != ERROR_MORE_DATA) {
            break;
        }
        free(users);
        users = NULL;
        if (needed > 65536) {
            error = ERROR_OUTOFMEMORY;
            break;
        }
        users = calloc(needed ? needed : 1, sizeof(*users));
        if (!users) {
            error = ERROR_OUTOFMEMORY;
            break;
        }
        count = needed;
    }
    if (error) {
        failure("RmGetList", error);
        goto cleanup;
    }
report:
    result = 0;
    for (UINT i = 0; i < count; i++) {
        char name[1024];
        if (!WideCharToMultiByte(CP_ACP, 0, users[i].application, -1, name,
                                 sizeof(name), NULL, NULL)) {
            strcpy(name, "unknown");
        }
        wpm_event_printf(WPM_EVENT_RESULT, "file-processes.user",
                         "file-processes", "list", "%lu\t%s\n",
                         (unsigned long)users[i].process.pid, name);
        if (kill && !wpm_is_readonly() && stop_user(&users[i])) {
            result = 1;
        }
    }
    if (kill && !wpm_is_readonly() && reasons) {
        result = failure("resources require reboot", ERROR_BUSY);
    }
    if (kill && !wpm_is_readonly() && !result) {
        if (legacy) {
            free(users);
            users = NULL;
            count = 0;
            error = legacy_list(paths, path_count, &users, &count);
        } else {
            error = list(session, &needed, &count, users, &reasons);
        }
        if (error || reasons) {
            result = failure("file users remain after termination",
                             error ? error : ERROR_BUSY);
        } else {
            /* Restart Manager can retain exited entries within a session. Only
             * a still-running instance with the same identity is a blocker. */
            for (UINT i = 0; i < count; i++) {
                FILETIME created, exited, kernel, cpu;
                HANDLE process =
                    OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE,
                                users[i].process.pid);
                if (!process) {
                    error = GetLastError();
                    if (error != ERROR_INVALID_PARAMETER) {
                        result = failure("verify remaining file user", error);
                    }
                    continue;
                }
                if (WaitForSingleObject(process, 0) != WAIT_OBJECT_0) {
                    if (!GetProcessTimes(process, &created, &exited, &kernel,
                                         &cpu)) {
                        result = failure("verify remaining file user",
                                         GetLastError());
                    } else if (!CompareFileTime(&created,
                                                &users[i].process.started)) {
                        result = failure("file users remain after termination",
                                         ERROR_BUSY);
                    }
                }
                CloseHandle(process);
            }
        }
    }
cleanup:
    if (active) {
        error = end(session);
        if (error) {
            result = failure("RmEndSession", error);
        }
    }
    if (library) {
        FreeLibrary(library);
    }
    free(users);
    for (int i = 0; i <= argc; i++) {
        free(paths[i]);
    }
    free(paths);
    return result;
}
