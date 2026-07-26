#include <errno.h>
#ifdef _CRTIMP
#undef _CRTIMP
#endif
#define _CRTIMP
#include <sys/utime.h>
#include <windows.h>

static int wpm_unix_time_to_file_time(__int64 value, FILETIME *file_time)
{
    ULARGE_INTEGER windows_time;
    const unsigned long long epoch_offset = 11644473600ULL;

    if (value < 0) return 0;
    windows_time.QuadPart = ((unsigned long long)value + epoch_offset) * 10000000ULL;
    file_time->dwLowDateTime = windows_time.LowPart;
    file_time->dwHighDateTime = windows_time.HighPart;
    return 1;
}

static int wpm_tcc_set_file_times(const char *path, __int64 access_value,
                                  __int64 modification_value, int use_current_time)
{
    FILETIME access_time;
    FILETIME modification_time;
    HANDLE file;
    BOOL result;

    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (use_current_time) {
        GetSystemTimeAsFileTime(&access_time);
        modification_time = access_time;
    } else if (!wpm_unix_time_to_file_time(access_value, &access_time) ||
               !wpm_unix_time_to_file_time(modification_value, &modification_time)) {
        errno = EINVAL;
        return -1;
    }

    file = CreateFileA(path, FILE_WRITE_ATTRIBUTES,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        errno = EACCES;
        return -1;
    }
    result = SetFileTime(file, NULL, &access_time, &modification_time);
    CloseHandle(file);
    if (!result) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int __cdecl _utime32(const char *path, struct __utimbuf32 *times)
{
    return wpm_tcc_set_file_times(path,
        times == NULL ? 0 : times->actime,
        times == NULL ? 0 : times->modtime,
        times == NULL);
}

int __cdecl _utime64(const char *path, struct __utimbuf64 *times)
{
    return wpm_tcc_set_file_times(path,
        times == NULL ? 0 : times->actime,
        times == NULL ? 0 : times->modtime,
        times == NULL);
}
